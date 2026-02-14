#include "engine/renderer/shader_library.h"
#include "engine/core/log.h"
#include "engine/core/time.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

namespace Engine {

// ── Init / Shutdown ─────────────────────────────────────────

void ShaderLibrary::Init(const std::string& shaderDir) {
    s_ShaderDir = shaderDir;
    // 确保目录存在
    if (!fs::exists(s_ShaderDir)) {
        fs::create_directories(s_ShaderDir);
        LOG_INFO("[ShaderLib] 创建着色器目录: %s", s_ShaderDir.c_str());
    }
    LOG_INFO("[ShaderLib] 初始化: %s", s_ShaderDir.c_str());
}

void ShaderLibrary::Shutdown() {
    LOG_INFO("[ShaderLib] 关闭 | %zu 个 Shader", s_Shaders.size());
    s_Shaders.clear();
    s_ReloadCallback = nullptr;
}

// ── 加载 ────────────────────────────────────────────────────

Ref<Shader> ShaderLibrary::Load(const std::string& name,
                                  const std::string& vertFile,
                                  const std::string& fragFile) {
    // 已缓存?
    auto it = s_Shaders.find(name);
    if (it != s_Shaders.end()) {
        return it->second.Program;
    }

    std::string vertPath = s_ShaderDir + "/" + vertFile;
    std::string fragPath = s_ShaderDir + "/" + fragFile;

    std::string vertSrc = ReadFile(vertPath);
    std::string fragSrc = ReadFile(fragPath);

    if (vertSrc.empty() || fragSrc.empty()) {
        LOG_ERROR("[ShaderLib] 加载失败: %s (%s, %s)", name.c_str(),
                  vertFile.c_str(), fragFile.c_str());
        return nullptr;
    }

    // #include 预处理
    std::unordered_set<std::string> included;
    vertSrc = Preprocess(vertSrc, s_ShaderDir, included);
    included.clear();
    fragSrc = Preprocess(fragSrc, s_ShaderDir, included);

    auto shader = std::make_shared<Shader>(vertSrc, fragSrc);
    if (!shader->IsValid()) {
        LOG_ERROR("[ShaderLib] 编译失败: %s", name.c_str());
        return nullptr;
    }

    ShaderEntry entry;
    entry.Name = name;
    entry.VertFile = vertFile;
    entry.FragFile = fragFile;
    entry.Program = shader;
    entry.LastModified = GetFileTime(vertPath);
    auto fragTime = GetFileTime(fragPath);
    if (fragTime > entry.LastModified) entry.LastModified = fragTime;

    s_Shaders[name] = std::move(entry);
    LOG_INFO("[ShaderLib] 加载: '%s' (%s + %s)", name.c_str(),
             vertFile.c_str(), fragFile.c_str());

    return shader;
}

Ref<Shader> ShaderLibrary::Get(const std::string& name) {
    auto it = s_Shaders.find(name);
    if (it != s_Shaders.end()) return it->second.Program;
    return nullptr;
}

// ── 热重载 ──────────────────────────────────────────────────

void ShaderLibrary::CheckHotReload() {
#ifdef ENGINE_DEBUG
    s_Timer += Time::DeltaTime();
    if (s_Timer < s_CheckInterval) return;
    s_Timer = 0;

    for (auto& [name, entry] : s_Shaders) {
        std::string vertPath = s_ShaderDir + "/" + entry.VertFile;
        std::string fragPath = s_ShaderDir + "/" + entry.FragFile;

        auto newTime = GetFileTime(vertPath);
        auto fragTime = GetFileTime(fragPath);
        if (fragTime > newTime) newTime = fragTime;

        if (newTime <= entry.LastModified) continue;

        LOG_INFO("[ShaderLib] 检测到修改, 重新编译: %s", name.c_str());

        std::string vertSrc = ReadFile(vertPath);
        std::string fragSrc = ReadFile(fragPath);
        if (vertSrc.empty() || fragSrc.empty()) continue;

        std::unordered_set<std::string> included;
        vertSrc = Preprocess(vertSrc, s_ShaderDir, included);
        included.clear();
        fragSrc = Preprocess(fragSrc, s_ShaderDir, included);

        auto newShader = std::make_shared<Shader>(vertSrc, fragSrc);
        if (newShader->IsValid()) {
            entry.Program = newShader;
            entry.LastModified = newTime;
            LOG_INFO("[ShaderLib] 热重载成功: %s", name.c_str());

            if (s_ReloadCallback) {
                s_ReloadCallback(name, newShader);
            }
        } else {
            LOG_ERROR("[ShaderLib] 热重载编译失败: %s (保留旧版本)", name.c_str());
        }
    }
#endif
}

void ShaderLibrary::SetReloadCallback(ReloadCallback cb) {
    s_ReloadCallback = std::move(cb);
}

// ── #include 预处理 ─────────────────────────────────────────

std::string ShaderLibrary::Preprocess(const std::string& source,
                                       const std::string& baseDir,
                                       std::unordered_set<std::string>& included) {
    std::istringstream stream(source);
    std::ostringstream result;
    std::string line;

    // 匹配 "#include "文件名""
    std::regex includeRegex(R"(^\s*#\s*include\s+\"([^\"]+)\"\s*$)");

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_match(line, match, includeRegex)) {
            std::string includeFile = match[1].str();
            std::string fullPath = baseDir + "/" + includeFile;

            if (included.count(fullPath)) {
                result << "// [ShaderLib] 跳过重复 #include: " << includeFile << "\n";
                continue;
            }
            included.insert(fullPath);

            std::string includeSrc = ReadFile(fullPath);
            if (includeSrc.empty()) {
                result << "// [ShaderLib] ERROR: 找不到 #include: " << includeFile << "\n";
                LOG_ERROR("[ShaderLib] #include 找不到: %s", fullPath.c_str());
            } else {
                result << "// --- BEGIN #include \"" << includeFile << "\" ---\n";
                result << Preprocess(includeSrc, baseDir, included);
                result << "// --- END #include \"" << includeFile << "\" ---\n";
            }
        } else {
            result << line << "\n";
        }
    }

    return result.str();
}

// ── 工具 ────────────────────────────────────────────────────

std::string ShaderLibrary::ReadFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::chrono::file_clock::time_point ShaderLibrary::GetFileTime(const std::string& filepath) {
    std::error_code ec;
    auto t = fs::last_write_time(filepath, ec);
    if (ec) return {};
    return t;
}

u32 ShaderLibrary::GetCount() {
    return (u32)s_Shaders.size();
}

} // namespace Engine
