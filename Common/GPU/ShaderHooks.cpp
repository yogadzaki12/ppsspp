#include "Common/GPU/ShaderHooks.h"

#include "Common/GPU/ShaderWriter.h"
#include "Common/File/FileUtil.h"
#include "Common/System/System.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string_view>

namespace ppsspp::shaderhooks {

namespace {

std::vector<ShaderHookDefinition> g_loadedHooks;
bool g_loadedHooksValid = false;

struct CommandSpec {
	const char *name;
	CommandType type;
	size_t argumentCount;
};

const std::array<CommandSpec, 26> kCommandSpecs = {{
	{"ReduceBloom", CommandType::ReduceBloom, 1},
	{"ClampLighting", CommandType::ClampLighting, 1},
	{"ReduceSpecular", CommandType::ReduceSpecular, 1},
	{"ReduceGlow", CommandType::ReduceGlow, 1},
	{"ReduceEmission", CommandType::ReduceEmission, 1},
	{"ReduceOverbright", CommandType::ReduceOverbright, 1},
	{"ReduceAdditiveBlend", CommandType::ReduceAdditiveBlend, 1},
	{"ReduceAlphaBlend", CommandType::ReduceAlphaBlend, 1},
	{"ForceOpaque", CommandType::ForceOpaque, 1},
	{"ReduceParticles", CommandType::ReduceParticles, 1},
	{"DisableParticles", CommandType::DisableParticles, 0},
	{"ReduceParticleBrightness", CommandType::ReduceParticleBrightness, 1},
	{"ReduceFogDensity", CommandType::ReduceFogDensity, 1},
	{"DisableFog", CommandType::DisableFog, 0},
	{"ClampFogBrightness", CommandType::ClampFogBrightness, 1},
	{"DesaturateTexture", CommandType::DesaturateTexture, 1},
	{"ReduceTextureBrightness", CommandType::ReduceTextureBrightness, 1},
	{"SharpenTexture", CommandType::SharpenTexture, 1},
	{"BlurTexture", CommandType::BlurTexture, 1},
	{"Saturation", CommandType::Saturation, 1},
	{"Contrast", CommandType::Contrast, 1},
	{"Gamma", CommandType::Gamma, 1},
	{"Tint", CommandType::Tint, 3},
	{"ClampHDR", CommandType::ClampHDR, 1},
	{"ReduceFramebufferGlow", CommandType::ReduceFramebufferGlow, 1},
	{"LimitFinalBrightness", CommandType::LimitFinalBrightness, 1},
}};

std::string Trim(std::string_view text) {
	const auto start = text.find_first_not_of(" \t\r\n");
	if (start == std::string_view::npos)
		return {};
	const auto end = text.find_last_not_of(" \t\r\n");
	return std::string(text.substr(start, end - start + 1));
}

std::string ToLower(std::string_view text) {
	std::string lower(text);
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return lower;
}

bool ParseBoolValue(const std::string &value, bool *out) {
	const std::string lowered = ToLower(Trim(value));
	if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on") {
		*out = true;
		return true;
	}
	if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "off") {
		*out = false;
		return true;
	}
	return false;
}

bool ParseFloatValue(const std::string &value, float *out) {
	char *end = nullptr;
	errno = 0;
	const float parsed = std::strtof(value.c_str(), &end);
	if (end == value.c_str() || errno == ERANGE)
		return false;
	while (*end != '\0') {
		if (!std::isspace(static_cast<unsigned char>(*end)))
			return false;
		++end;
	}
	if (!std::isfinite(parsed))
		return false;
	*out = parsed;
	return true;
}

std::vector<std::string> SplitArguments(const std::string &arguments) {
	std::vector<std::string> result;
	std::string current;
	for (char ch : arguments) {
		if (ch == ',') {
			result.push_back(Trim(current));
			current.clear();
		} else {
			current.push_back(ch);
		}
	}
	if (!current.empty() || !arguments.empty())
		result.push_back(Trim(current));
	return result;
}

const CommandSpec *FindCommandSpec(std::string_view name) {
	const std::string lowered = ToLower(name);
	for (const auto &spec : kCommandSpecs) {
		if (ToLower(spec.name) == lowered)
			return &spec;
	}
	return nullptr;
}

std::string FormatFloat(float value) {
	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << value;
	return out.str();
}

std::string JoinArguments(const std::vector<float> &arguments) {
	std::ostringstream out;
	for (size_t i = 0; i < arguments.size(); ++i) {
		if (i != 0)
			out << ", ";
		out << FormatFloat(arguments[i]);
	}
	return out.str();
}

void LoadHooksFromPath(const HookParser &parser, const std::filesystem::path &root, bool recursive,
	std::set<std::string> *seenFiles, std::vector<HookParseResult> *results) {
	if (root.empty() || !std::filesystem::exists(root))
		return;

	HookFileLoader loader(root);
	auto loadedResults = loader.LoadAll(parser, recursive);
	for (auto &result : loadedResults) {
		std::string normalizedPath = result.hook.sourcePath.lexically_normal().string();
		if (seenFiles->insert(normalizedPath).second)
			results->push_back(std::move(result));
	}
}

void LogShaderHookExecution(const std::string &hookName, const std::string &gameId, 
	const char *colorVar, HookPoint point, const std::vector<std::unique_ptr<ShaderHookCommand>> &commands) {
	try {
		const Path logFile = GetSysDirectory(DIRECTORY_SYSTEM) / "shader_hooks.log";
		std::ofstream logStream(logFile.c_str(), std::ios::app);
		if (logStream.is_open()) {
			logStream << "\n[SHADER EXECUTION] Hook: " << hookName << " (GameID: " << gameId 
				<< ", Point: " << ToString(point) << ", ColorVar: " << colorVar << ")\n";
			logStream << "  Executing " << commands.size() << " command(s) in Fragment Shader:\n";
			for (size_t i = 0; i < commands.size(); ++i) {
				if (commands[i]) {
					logStream << "    [" << (i + 1) << "] " << commands[i]->Describe() << "\n";
				}
			}
			logStream.flush();
		}
	} catch (...) {
		// Silently ignore logging errors to avoid disrupting shader generation
	}
}

bool ParseHookSectionLine(HookContext &context, const std::string &line, std::string *error) {
	const auto equals = line.find('=');
	if (equals == std::string::npos) {
		*error = "Expected key=value pair in hook metadata: " + line;
		return false;
	}

	const std::string key = ToLower(Trim(std::string_view(line).substr(0, equals)));
	const std::string value = Trim(std::string_view(line).substr(equals + 1));

	if (key == "name") {
		context.hookName = value;
		return true;
	}
	if (key == "author") {
		context.author = value;
		return true;
	}
	if (key == "gameid") {
		context.gameId = value;
		return true;
	}
	if (key == "stage") {
		const std::string stage = ToLower(value);
		if (stage == "vertex") {
			context.stage = ShaderStage::Vertex;
			return true;
		}
		if (stage == "fragment") {
			context.stage = ShaderStage::Fragment;
			return true;
		}
		*error = "Unknown stage: " + value;
		return false;
	}
	if (key == "point") {
		const std::string point = ToLower(value);
		if (point == "beforetexture") context.point = HookPoint::BeforeTexture;
		else if (point == "aftertexture") context.point = HookPoint::AfterTexture;
		else if (point == "beforelighting") context.point = HookPoint::BeforeLighting;
		else if (point == "afterlighting") context.point = HookPoint::AfterLighting;
		else if (point == "beforefog") context.point = HookPoint::BeforeFog;
		else if (point == "afterfog") context.point = HookPoint::AfterFog;
		else if (point == "beforeblend") context.point = HookPoint::BeforeBlend;
		else if (point == "afterblend") context.point = HookPoint::AfterBlend;
		else if (point == "beforefinalcolor") context.point = HookPoint::BeforeFinalColor;
		else {
			*error = "Unknown hook point: " + value;
			return false;
		}
		return true;
	}
	if (key == "enabled") {
		if (!ParseBoolValue(value, &context.enabled)) {
			*error = "Invalid enabled value: " + value;
			return false;
		}
		return true;
	}

	*error = "Unknown hook metadata key: " + key;
	return false;
}

bool ParseCommandLine(const std::string &line, std::unique_ptr<ShaderHookCommand> *command, std::string *error) {
	std::string commandText = Trim(line);
	if (!commandText.empty() && commandText.back() == ';')
		commandText.pop_back();

	const auto open = commandText.find('(');
	const auto close = commandText.rfind(')');
	if (open == std::string::npos || close == std::string::npos || close < open) {
		*error = "Expected helper command syntax: Name(arg1, arg2);";
		return false;
	}

	const std::string name = Trim(std::string_view(commandText).substr(0, open));
	const std::string argumentText = Trim(std::string_view(commandText).substr(open + 1, close - open - 1));
	const CommandSpec *spec = FindCommandSpec(name);
	if (!spec) {
		*error = "Unknown helper command: " + name;
		return false;
	}

	std::vector<std::string> argumentStrings;
	if (!argumentText.empty())
		argumentStrings = SplitArguments(argumentText);

	if (argumentStrings.size() != spec->argumentCount) {
		std::ostringstream message;
		message << spec->name << " expects " << spec->argumentCount << " argument(s), got " << argumentStrings.size();
		*error = message.str();
		return false;
	}

	std::vector<float> arguments;
	arguments.reserve(argumentStrings.size());
	for (const std::string &argumentString : argumentStrings) {
		float value = 0.0f;
		if (!ParseFloatValue(argumentString, &value)) {
			*error = "Invalid numeric argument for " + std::string(spec->name) + ": " + argumentString;
			return false;
		}
		arguments.push_back(value);
	}

	*command = std::make_unique<ParsedShaderHookCommand>(spec->type, spec->name, std::move(arguments));
	return true;
}

void WriteCommandTransform(ShaderWriter &writer, const ShaderHookCommand &command, const char *colorVar) {
	const auto &arguments = command.Arguments();
	switch (command.Type()) {
	case CommandType::ReduceBloom:
	case CommandType::ReduceSpecular:
	case CommandType::ReduceGlow:
	case CommandType::ReduceEmission:
	case CommandType::ReduceOverbright:
	case CommandType::ReduceAdditiveBlend:
	case CommandType::ReduceAlphaBlend:
	case CommandType::ReduceParticles:
	case CommandType::ReduceParticleBrightness:
	case CommandType::ReduceFogDensity:
	case CommandType::ReduceTextureBrightness:
	case CommandType::ReduceFramebufferGlow:
	case CommandType::LimitFinalBrightness:
		writer.F("  %s.rgb *= %.2ff;\n", colorVar, arguments[0]);
		break;
	case CommandType::ClampLighting:
	case CommandType::ClampFogBrightness:
	case CommandType::ClampHDR:
		writer.F("  %s.rgb = min(%s.rgb, vec3(%.2ff));\n", colorVar, colorVar, arguments[0]);
		break;
	case CommandType::ForceOpaque:
		writer.F("  %s.a = %.2ff;\n", colorVar, arguments[0]);
		break;
	case CommandType::DisableParticles:
	case CommandType::DisableFog:
		writer.F("  %s.rgb *= 0.0;\n", colorVar);
		break;
	case CommandType::DesaturateTexture:
		writer.F("  %s.rgb = mix(%s.rgb, vec3(dot(%s.rgb, vec3(0.299, 0.587, 0.114))), %.2ff);\n", colorVar, colorVar, colorVar, arguments[0]);
		break;
	case CommandType::SharpenTexture:
		writer.F("  %s.rgb = clamp(%s.rgb * (1.0 + %.2ff), 0.0, 1.0);\n", colorVar, colorVar, arguments[0]);
		break;
	case CommandType::BlurTexture:
		writer.F("  %s.rgb = mix(%s.rgb, vec3(dot(%s.rgb, vec3(0.3333))), %.2ff);\n", colorVar, colorVar, colorVar, arguments[0]);
		break;
	case CommandType::Saturation:
		writer.F(
			"  %s.rgb = mix(vec3(dot(%s.rgb, vec3(0.299, 0.587, 0.114))), %s.rgb, %.2ff);\n",
			colorVar, colorVar, colorVar, arguments[0]);
		break;
	case CommandType::Contrast:
		writer.F("  %s.rgb = clamp((%s.rgb - 0.5) * %.2ff + 0.5, 0.0, 1.0);\n", colorVar, colorVar, arguments[0]);
		break;
	case CommandType::Gamma:
		writer.F("  %s.rgb = pow(max(%s.rgb, vec3(0.0)), vec3(1.0 / max(%.4ff, 0.0001)));\n", colorVar, colorVar, arguments[0]);
		break;
	case CommandType::Tint:
		writer.F("  %s.rgb *= vec3(%.2ff, %.2ff, %.2ff);\n", colorVar, arguments[0], arguments[1], arguments[2]);
		break;
	default:
		writer.F("  // Unsupported hook command: %s\n", command.Name().c_str());
		break;
	}
}

}  // namespace

ParsedShaderHookCommand::ParsedShaderHookCommand(CommandType type, std::string name, std::vector<float> arguments)
	: type_(type), name_(std::move(name)), arguments_(std::move(arguments)) {}

CommandType ParsedShaderHookCommand::Type() const {
	return type_;
}

const std::string &ParsedShaderHookCommand::Name() const {
	return name_;
}

const std::vector<float> &ParsedShaderHookCommand::Arguments() const {
	return arguments_;
}

bool ParsedShaderHookCommand::Validate(std::string *error) const {
	const CommandSpec *spec = FindCommandSpec(name_);
	if (!spec) {
		*error = "Unknown helper command: " + name_;
		return false;
	}
	if (spec->type != type_) {
		*error = "Command registry/type mismatch for: " + name_;
		return false;
	}
	if (arguments_.size() != spec->argumentCount) {
		std::ostringstream message;
		message << name_ << " expects " << spec->argumentCount << " argument(s), got " << arguments_.size();
		*error = message.str();
		return false;
	}
	for (float argument : arguments_) {
		if (!std::isfinite(argument)) {
			*error = "Command arguments must be finite numbers: " + name_;
			return false;
		}
	}
	return true;
}

std::string ParsedShaderHookCommand::Describe() const {
	std::ostringstream out;
	out << name_ << '(' << JoinArguments(arguments_) << ')';
	return out.str();
}

void ParsedShaderHookCommand::Execute(std::ostream &out) const {
	out << "- " << Describe() << '\n';
}

const char *ToString(ShaderStage stage) {
	switch (stage) {
	case ShaderStage::Vertex: return "Vertex";
	case ShaderStage::Fragment: return "Fragment";
	}
	return "Unknown";
}

const char *ToString(HookPoint point) {
	switch (point) {
	case HookPoint::BeforeTexture: return "BeforeTexture";
	case HookPoint::AfterTexture: return "AfterTexture";
	case HookPoint::BeforeLighting: return "BeforeLighting";
	case HookPoint::AfterLighting: return "AfterLighting";
	case HookPoint::BeforeFog: return "BeforeFog";
	case HookPoint::AfterFog: return "AfterFog";
	case HookPoint::BeforeBlend: return "BeforeBlend";
	case HookPoint::AfterBlend: return "AfterBlend";
	case HookPoint::BeforeFinalColor: return "BeforeFinalColor";
	}
	return "Unknown";
}

const char *ToString(CommandType type) {
	switch (type) {
	case CommandType::ReduceBloom: return "ReduceBloom";
	case CommandType::ClampLighting: return "ClampLighting";
	case CommandType::ReduceSpecular: return "ReduceSpecular";
	case CommandType::ReduceGlow: return "ReduceGlow";
	case CommandType::ReduceEmission: return "ReduceEmission";
	case CommandType::ReduceOverbright: return "ReduceOverbright";
	case CommandType::ReduceAdditiveBlend: return "ReduceAdditiveBlend";
	case CommandType::ReduceAlphaBlend: return "ReduceAlphaBlend";
	case CommandType::ForceOpaque: return "ForceOpaque";
	case CommandType::ReduceParticles: return "ReduceParticles";
	case CommandType::DisableParticles: return "DisableParticles";
	case CommandType::ReduceParticleBrightness: return "ReduceParticleBrightness";
	case CommandType::ReduceFogDensity: return "ReduceFogDensity";
	case CommandType::DisableFog: return "DisableFog";
	case CommandType::ClampFogBrightness: return "ClampFogBrightness";
	case CommandType::DesaturateTexture: return "DesaturateTexture";
	case CommandType::ReduceTextureBrightness: return "ReduceTextureBrightness";
	case CommandType::SharpenTexture: return "SharpenTexture";
	case CommandType::BlurTexture: return "BlurTexture";
	case CommandType::Saturation: return "Saturation";
	case CommandType::Contrast: return "Contrast";
	case CommandType::Gamma: return "Gamma";
	case CommandType::Tint: return "Tint";
	case CommandType::ClampHDR: return "ClampHDR";
	case CommandType::ReduceFramebufferGlow: return "ReduceFramebufferGlow";
	case CommandType::LimitFinalBrightness: return "LimitFinalBrightness";
	default: return "Unknown";
	}
}

HookParseResult HookParser::ParseFile(const std::filesystem::path &path) const {
	std::ifstream file(path, std::ios::in | std::ios::binary);
	if (!file) {
		return {false, "Unable to open hook file: " + path.string(), {}};
	}

	std::ostringstream contents;
	contents << file.rdbuf();
	return ParseText(contents.str(), path);
}

HookParseResult HookParser::ParseText(const std::string &text, const std::filesystem::path &sourcePath) const {
	HookParseResult result;
	result.hook.sourcePath = sourcePath;

	enum class Section {
		None,
		Hook,
		Code,
	};

	Section section = Section::None;
	HookContext context;
	std::vector<std::unique_ptr<ShaderHookCommand>> commands;
	std::string error;

	std::istringstream input(text);
	std::string rawLine;
	size_t lineNumber = 0;
	while (std::getline(input, rawLine)) {
		++lineNumber;
		const std::string line = Trim(rawLine);
		if (line.empty())
			continue;

		const std::string lowered = ToLower(line);
		if (lowered == "// ==hook==") {
			section = Section::Hook;
			continue;
		}
		if (lowered == "// ==code==") {
			section = Section::Code;
			continue;
		}
		if (line.rfind("//", 0) == 0)
			continue;

		if (section == Section::Hook) {
			if (!ParseHookSectionLine(context, line, &error)) {
				result.error = sourcePath.empty()
					? ("Line " + std::to_string(lineNumber) + ": " + error)
					: (sourcePath.string() + ":" + std::to_string(lineNumber) + ": " + error);
				return result;
			}
			continue;
		}

		if (section == Section::Code) {
			std::unique_ptr<ShaderHookCommand> command;
			if (!ParseCommandLine(line, &command, &error)) {
				result.error = sourcePath.empty()
					? ("Line " + std::to_string(lineNumber) + ": " + error)
					: (sourcePath.string() + ":" + std::to_string(lineNumber) + ": " + error);
				return result;
			}
			commands.push_back(std::move(command));
			continue;
		}

		result.error = sourcePath.empty()
			? ("Line " + std::to_string(lineNumber) + ": content found before section marker")
			: (sourcePath.string() + ":" + std::to_string(lineNumber) + ": content found before section marker");
		return result;
	}

	if (context.hookName.empty()) {
		result.error = "Missing Name metadata";
		return result;
	}
	if (context.author.empty()) {
		result.error = "Missing Author metadata";
		return result;
	}
	if (context.gameId.empty()) {
		result.error = "Missing GameID metadata";
		return result;
	}

	for (const auto &command : commands) {
		if (!command)
			continue;
		std::string commandError;
		if (!command->Validate(&commandError)) {
			result.error = commandError;
			return result;
		}
	}

	result.hook.context = std::move(context);
	result.hook.commands = std::move(commands);
	result.success = true;
	return result;
}

HookFileLoader::HookFileLoader(std::filesystem::path root) : root_(std::move(root)) {}

std::vector<std::filesystem::path> HookFileLoader::Scan(bool recursive) const {
	std::vector<std::filesystem::path> files;
	if (root_.empty() || !std::filesystem::exists(root_))
		return files;

	if (recursive) {
		for (const auto &entry : std::filesystem::recursive_directory_iterator(root_)) {
			if (entry.is_regular_file() && entry.path().extension() == ".hook")
				files.push_back(entry.path());
		}
	} else {
		for (const auto &entry : std::filesystem::directory_iterator(root_)) {
			if (entry.is_regular_file() && entry.path().extension() == ".hook")
				files.push_back(entry.path());
		}
	}

	std::sort(files.begin(), files.end());
	return files;
}

std::vector<HookParseResult> HookFileLoader::LoadAll(const HookParser &parser, bool recursive) const {
	std::vector<HookParseResult> results;
	for (const auto &path : Scan(recursive))
		results.push_back(parser.ParseFile(path));
	return results;
}

std::vector<HookParseResult> HookFileLoader::ScanAndLoadDefaultPaths(const HookParser &parser, bool recursive) {
	std::vector<HookParseResult> allResults;
	std::set<std::string> seenFiles;  // Track by normalized path string
	std::vector<std::filesystem::path> searchPaths = {
		"shaders",
	};

	for (const auto &searchPath : searchPaths) {
		LoadHooksFromPath(parser, searchPath, recursive, &seenFiles, &allResults);
	}

	return allResults;
}

std::vector<HookParseResult> LoadShaderHooksFromDisk(bool recursive) {
	HookParser parser;
	auto results = HookFileLoader::ScanAndLoadDefaultPaths(parser, recursive);
	g_loadedHooks.clear();
	for (auto &result : results) {
		if (result.success)
			g_loadedHooks.push_back(std::move(result.hook));
	}
	g_loadedHooksValid = true;
	return results;
}

std::vector<HookParseResult> LoadShaderHooksFromDisk(const std::filesystem::path &basePath, bool recursive) {
	// The caller may pass either the actual shaders directory or the PSP
	// memstick root. Try multiple sensible locations:
	//  - basePath itself
	//  - basePath/shaders (lowercase)
	//  - basePath/SHADERS (uppercase) for users who manually named it so
	HookParser parser;
	std::vector<HookParseResult> results;
	std::set<std::string> seenFiles;

	// Try the provided path first.
	LoadHooksFromPath(parser, basePath, recursive, &seenFiles, &results);

	// If basePath doesn't look like a shaders folder, also try appending
	// "shaders" and "SHADERS" so both common cases are covered.
	auto tryAppend = [&](const std::string &sub) {
		std::filesystem::path p = basePath / sub;
		LoadHooksFromPath(parser, p, recursive, &seenFiles, &results);
	};

	// Only append if basePath filename is not already 'shaders' (case-insensitively).
	std::string fname = basePath.filename().string();
	auto iequals = [](const std::string &a, const std::string &b) {
		if (a.size() != b.size()) return false;
		for (size_t i = 0; i < a.size(); ++i)
			if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
				return false;
		return true;
	};
	if (!iequals(fname, "shaders")) {
		tryAppend("shaders");
		tryAppend("SHADERS");
	}

	g_loadedHooks.clear();
	for (auto &result : results) {
		if (result.success)
			g_loadedHooks.push_back(std::move(result.hook));
	}
	g_loadedHooksValid = true;
	return results;
}

const std::vector<ShaderHookDefinition> &GetLoadedShaderHooks() {
	return g_loadedHooks;
}

void WriteFragmentHookTransforms(ShaderWriter &writer, std::string_view gameId, const char *colorVar) {
	// Backwards-compatible: default to BeforeFinalColor
	WriteFragmentHookTransforms(writer, gameId, colorVar, HookPoint::BeforeFinalColor);
}

void WriteFragmentHookTransforms(ShaderWriter &writer, std::string_view gameId, const char *colorVar, HookPoint point) {
	if (!g_loadedHooksValid)
		return;

	bool wroteAny = false;
	for (const auto &hook : g_loadedHooks) {
		if (!hook.context.enabled)
			continue;
		if (hook.context.stage != ShaderStage::Fragment)
			continue;
		if (hook.context.point != point)
			continue;
		if (!hook.context.gameId.empty() && hook.context.gameId != "*" && !gameId.empty() && hook.context.gameId != gameId)
			continue;

		if (!wroteAny) {
			writer.F("  // Shader hook transforms (%s)\n", ToString(point));
			wroteAny = true;
		}

		writer.F("  // Hook: %s (%s)\n", hook.context.hookName.c_str(), hook.context.gameId.c_str());
		
		// Log shader hook execution
		LogShaderHookExecution(hook.context.hookName, hook.context.gameId, colorVar, point, hook.commands);
		
		for (const auto &command : hook.commands) {
			if (command)
				WriteCommandTransform(writer, *command, colorVar);
		}
	}
}

void ShaderHookExecutor::Execute(const ShaderHookDefinition &hook, std::ostream &out) const {
	out << "[ShaderHook]\n";
	out << "Loaded: " << hook.context.hookName << "\n\n";

	if (!hook.context.enabled) {
		out << "Hook is disabled, skipping execution.\n";
		return;
	}

	out << "Applying hook:\n";
	out << "GameID=" << hook.context.gameId << '\n';
	out << "Stage=" << ToString(hook.context.stage) << '\n';
	out << "Point=" << ToString(hook.context.point) << "\n\n";

	out << "Executing:\n\n";
	for (const auto &command : hook.commands) {
		if (command)
			command->Execute(out);
	}
}

}  // namespace ppsspp::shaderhooks

// Vertex-stage hook writer implementation
namespace ppsspp::shaderhooks {

void WriteVertexHookTransforms(ShaderWriter &writer, std::string_view gameId, const char *varName, HookPoint point) {
	if (!g_loadedHooksValid)
		return;

	bool wroteAny = false;
	for (const auto &hook : g_loadedHooks) {
		if (!hook.context.enabled)
			continue;
		if (hook.context.stage != ShaderStage::Vertex)
			continue;
		if (hook.context.point != point)
			continue;
		if (!hook.context.gameId.empty() && hook.context.gameId != "*" && !gameId.empty() && hook.context.gameId != gameId)
			continue;

		if (!wroteAny) {
			writer.F("  // Vertex shader hook transforms (%s)\n", ToString(point));
			wroteAny = true;
		}

		writer.F("  // Hook: %s (%s)\n", hook.context.hookName.c_str(), hook.context.gameId.c_str());
		
		// Log shader hook execution (Vertex stage)
		LogShaderHookExecution(hook.context.hookName, hook.context.gameId, varName, point, hook.commands);
		
		for (const auto &command : hook.commands) {
			if (command)
				WriteCommandTransform(writer, *command, varName);
		}
	}
}

} // namespace ppsspp::shaderhooks