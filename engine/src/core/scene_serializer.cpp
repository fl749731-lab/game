#include "engine/core/scene_serializer.h"
#include "engine/core/log.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace Engine {

// ═══════════════════════════════════════════════════════════════
// JSON 生成辅助
// ═══════════════════════════════════════════════════════════════

class JsonWriter {
public:
    JsonWriter() { m_SS << std::fixed << std::setprecision(4); }

    void BeginObject() { m_SS << "{"; m_Stack.push_back('{'); m_First.push_back(true); }
    void EndObject()   { m_Stack.pop_back(); m_First.pop_back(); m_SS << "}"; }
    void BeginArray()  { m_SS << "["; m_Stack.push_back('['); m_First.push_back(true); }
    void EndArray()    { m_Stack.pop_back(); m_First.pop_back(); m_SS << "]"; }

    void Key(const std::string& key) {
        Comma();
        m_SS << "\"" << key << "\":";
    }

    void ValueStr(const std::string& val) {
        if (m_Stack.back() == '[') Comma();
        m_SS << "\"" << Escape(val) << "\"";
    }

    void ValueF32(f32 val) {
        if (m_Stack.back() == '[') Comma();
        // 整数按整数输出避免干扰
        if (val == std::floor(val) && std::abs(val) < 1e7f)
            m_SS << (i64)val;
        else
            m_SS << val;
    }

    void ValueI32(i32 val) {
        if (m_Stack.back() == '[') Comma();
        m_SS << val;
    }

    void ValueBool(bool val) {
        if (m_Stack.back() == '[') Comma();
        m_SS << (val ? "true" : "false");
    }

    void KeyStr(const std::string& k, const std::string& v) { Key(k); ValueStr(v); }
    void KeyF32(const std::string& k, f32 v)                { Key(k); ValueF32(v); }
    void KeyI32(const std::string& k, i32 v)                { Key(k); ValueI32(v); }
    void KeyBool(const std::string& k, bool v)               { Key(k); ValueBool(v); }

    void KeyVec3(const std::string& k, const glm::vec3& v) {
        Key(k); BeginArray();
        ValueF32(v.x); ValueF32(v.y); ValueF32(v.z);
        EndArray();
    }

    std::string GetString() const { return m_SS.str(); }

private:
    void Comma() {
        if (!m_First.back()) m_SS << ",";
        else m_First.back() = false;
    }

    static std::string Escape(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else out += c;
        }
        return out;
    }

    std::stringstream m_SS;
    std::vector<char> m_Stack;
    std::vector<bool> m_First;
};

// ═══════════════════════════════════════════════════════════════
// JSON 解析辅助 (简易 tokenizer)
// ═══════════════════════════════════════════════════════════════

enum class TokenType { LBrace, RBrace, LBracket, RBracket, Colon, Comma,
                       String, Number, Bool, Null, End };

struct Token {
    TokenType Type;
    std::string Str;
    f64 Num = 0;
    bool BoolVal = false;
};

class JsonParser {
public:
    JsonParser(const std::string& json) : m_Src(json), m_Pos(0) {}

    Token Next() {
        SkipWhitespace();
        if (m_Pos >= m_Src.size()) return {TokenType::End};

        char c = m_Src[m_Pos];
        switch (c) {
            case '{': m_Pos++; return {TokenType::LBrace};
            case '}': m_Pos++; return {TokenType::RBrace};
            case '[': m_Pos++; return {TokenType::LBracket};
            case ']': m_Pos++; return {TokenType::RBracket};
            case ':': m_Pos++; return {TokenType::Colon};
            case ',': m_Pos++; return {TokenType::Comma};
            case '"': return ReadString();
            default:
                if (c == '-' || (c >= '0' && c <= '9')) return ReadNumber();
                if (c == 't' || c == 'f') return ReadBool();
                if (c == 'n') { m_Pos += 4; return {TokenType::Null}; }
                m_Pos++;
                return {TokenType::End};
        }
    }

    Token Peek() {
        size_t saved = m_Pos;
        auto t = Next();
        m_Pos = saved;
        return t;
    }

    void Expect(TokenType type) {
        auto t = Next();
        if (t.Type != type) {
            LOG_ERROR("[SceneSerializer] JSON 解析错误: 期望 token %d, 得到 %d (pos=%zu)",
                      (int)type, (int)t.Type, m_Pos);
        }
    }

    std::string ExpectStr() { auto t = Next(); return t.Str; }
    f64 ExpectNum()         { auto t = Next(); return t.Num; }
    bool ExpectBool()       { auto t = Next(); return t.BoolVal; }

    // 读取 [x, y, z] 数组
    glm::vec3 ReadVec3() {
        Expect(TokenType::LBracket);
        f32 x = (f32)ExpectNum(); Expect(TokenType::Comma);
        f32 y = (f32)ExpectNum(); Expect(TokenType::Comma);
        f32 z = (f32)ExpectNum();
        Expect(TokenType::RBracket);
        return {x, y, z};
    }

    bool IsEnd() { SkipWhitespace(); return m_Pos >= m_Src.size(); }

private:
    void SkipWhitespace() {
        while (m_Pos < m_Src.size() && 
               (m_Src[m_Pos] == ' ' || m_Src[m_Pos] == '\n' ||
                m_Src[m_Pos] == '\r' || m_Src[m_Pos] == '\t'))
            m_Pos++;
    }

    Token ReadString() {
        m_Pos++; // skip opening "
        std::string s;
        while (m_Pos < m_Src.size() && m_Src[m_Pos] != '"') {
            if (m_Src[m_Pos] == '\\') {
                m_Pos++;
                if (m_Pos < m_Src.size()) {
                    switch (m_Src[m_Pos]) {
                        case '"':  s += '"'; break;
                        case '\\': s += '\\'; break;
                        case 'n':  s += '\n'; break;
                        default:   s += m_Src[m_Pos]; break;
                    }
                }
            } else {
                s += m_Src[m_Pos];
            }
            m_Pos++;
        }
        if (m_Pos < m_Src.size()) m_Pos++; // skip closing "
        return {TokenType::String, s};
    }

    Token ReadNumber() {
        size_t start = m_Pos;
        if (m_Src[m_Pos] == '-') m_Pos++;
        while (m_Pos < m_Src.size() && ((m_Src[m_Pos] >= '0' && m_Src[m_Pos] <= '9') || m_Src[m_Pos] == '.'))
            m_Pos++;
        // 科学计数法
        if (m_Pos < m_Src.size() && (m_Src[m_Pos] == 'e' || m_Src[m_Pos] == 'E')) {
            m_Pos++;
            if (m_Pos < m_Src.size() && (m_Src[m_Pos] == '+' || m_Src[m_Pos] == '-')) m_Pos++;
            while (m_Pos < m_Src.size() && m_Src[m_Pos] >= '0' && m_Src[m_Pos] <= '9') m_Pos++;
        }
        std::string numStr = m_Src.substr(start, m_Pos - start);
        Token t;
        t.Type = TokenType::Number;
        t.Num = std::stod(numStr);
        return t;
    }

    Token ReadBool() {
        Token t;
        t.Type = TokenType::Bool;
        if (m_Src.substr(m_Pos, 4) == "true") {
            t.BoolVal = true;  m_Pos += 4;
        } else {
            t.BoolVal = false; m_Pos += 5;
        }
        return t;
    }

    std::string m_Src;
    size_t m_Pos;
};

// ═══════════════════════════════════════════════════════════════
// 保存
// ═══════════════════════════════════════════════════════════════

bool SceneSerializer::Save(const Scene& scene, const std::string& filepath) {
    JsonWriter w;
    w.BeginObject();

    w.KeyStr("name", scene.GetName());

    // ── 光照 ────────────────────────────────────────────────
    w.Key("directionalLight"); w.BeginObject();
    {
        auto& dl = const_cast<Scene&>(scene).GetDirLight();
        w.KeyVec3("direction", dl.Direction);
        w.KeyVec3("color", dl.Color);
        w.KeyF32("intensity", dl.Intensity);
    }
    w.EndObject();

    w.Key("pointLights"); w.BeginArray();
    for (auto& pl : const_cast<Scene&>(scene).GetPointLights()) {
        w.BeginObject();
        w.KeyVec3("position", pl.Position);
        w.KeyVec3("color", pl.Color);
        w.KeyF32("intensity", pl.Intensity);
        w.KeyF32("constant", pl.Constant);
        w.KeyF32("linear", pl.Linear);
        w.KeyF32("quadratic", pl.Quadratic);
        w.EndObject();
    }
    w.EndArray();

    w.Key("spotLights"); w.BeginArray();
    for (auto& sl : const_cast<Scene&>(scene).GetSpotLights()) {
        w.BeginObject();
        w.KeyVec3("position", sl.Position);
        w.KeyVec3("direction", sl.Direction);
        w.KeyVec3("color", sl.Color);
        w.KeyF32("intensity", sl.Intensity);
        w.KeyF32("innerCutoff", sl.InnerCutoff);
        w.KeyF32("outerCutoff", sl.OuterCutoff);
        w.KeyF32("constant", sl.Constant);
        w.KeyF32("linear", sl.Linear);
        w.KeyF32("quadratic", sl.Quadratic);
        w.EndObject();
    }
    w.EndArray();

    // ── 实体 ────────────────────────────────────────────────
    w.Key("entities"); w.BeginArray();

    auto& world = const_cast<Scene&>(scene).GetWorld();
    for (auto e : world.GetEntities()) {
        w.BeginObject();
        w.KeyI32("id", (i32)e);

        // Tag
        auto* tag = world.GetComponent<TagComponent>(e);
        if (tag) {
            w.Key("tag"); w.BeginObject();
            w.KeyStr("name", tag->Name);
            w.EndObject();
        }

        // Transform
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (tr) {
            w.Key("transform"); w.BeginObject();
            w.KeyF32("x", tr->X); w.KeyF32("y", tr->Y); w.KeyF32("z", tr->Z);
            w.KeyF32("rotX", tr->RotX); w.KeyF32("rotY", tr->RotY); w.KeyF32("rotZ", tr->RotZ);
            w.KeyF32("scaleX", tr->ScaleX); w.KeyF32("scaleY", tr->ScaleY); w.KeyF32("scaleZ", tr->ScaleZ);
            w.EndObject();
        }

        // Render
        auto* rc = world.GetComponent<RenderComponent>(e);
        if (rc) {
            w.Key("render"); w.BeginObject();
            w.KeyStr("meshType", rc->MeshType);
            if (!rc->ObjPath.empty()) w.KeyStr("objPath", rc->ObjPath);
            w.KeyF32("colorR", rc->ColorR); w.KeyF32("colorG", rc->ColorG); w.KeyF32("colorB", rc->ColorB);
            w.KeyF32("shininess", rc->Shininess);
            w.EndObject();
        }

        // Material
        auto* mat = world.GetComponent<MaterialComponent>(e);
        if (mat) {
            w.Key("material"); w.BeginObject();
            w.KeyF32("diffuseR", mat->DiffuseR); w.KeyF32("diffuseG", mat->DiffuseG); w.KeyF32("diffuseB", mat->DiffuseB);
            w.KeyF32("specularR", mat->SpecularR); w.KeyF32("specularG", mat->SpecularG); w.KeyF32("specularB", mat->SpecularB);
            w.KeyF32("shininess", mat->Shininess);
            w.KeyF32("roughness", mat->Roughness);
            w.KeyF32("metallic", mat->Metallic);
            if (!mat->TextureName.empty()) w.KeyStr("textureName", mat->TextureName);
            if (!mat->NormalMapName.empty()) w.KeyStr("normalMapName", mat->NormalMapName);
            w.KeyBool("emissive", mat->Emissive);
            if (mat->Emissive) {
                w.KeyF32("emissiveR", mat->EmissiveR);
                w.KeyF32("emissiveG", mat->EmissiveG);
                w.KeyF32("emissiveB", mat->EmissiveB);
                w.KeyF32("emissiveIntensity", mat->EmissiveIntensity);
            }
            w.EndObject();
        }

        // Health
        auto* hp = world.GetComponent<HealthComponent>(e);
        if (hp) {
            w.Key("health"); w.BeginObject();
            w.KeyF32("current", hp->Current);
            w.KeyF32("max", hp->Max);
            w.EndObject();
        }

        // Velocity
        auto* vel = world.GetComponent<VelocityComponent>(e);
        if (vel) {
            w.Key("velocity"); w.BeginObject();
            w.KeyF32("vx", vel->VX); w.KeyF32("vy", vel->VY); w.KeyF32("vz", vel->VZ);
            w.EndObject();
        }

        // AI
        auto* ai = world.GetComponent<AIComponent>(e);
        if (ai) {
            w.Key("ai"); w.BeginObject();
            w.KeyStr("scriptModule", ai->ScriptModule);
            w.KeyStr("state", ai->State);
            w.KeyF32("detectRange", ai->DetectRange);
            w.KeyF32("attackRange", ai->AttackRange);
            w.EndObject();
        }

        // Lifetime
        auto* lt = world.GetComponent<LifetimeComponent>(e);
        if (lt) {
            w.Key("lifetime"); w.BeginObject();
            w.KeyF32("timeRemaining", lt->TimeRemaining);
            w.EndObject();
        }

        w.EndObject();
    }
    w.EndArray();

    w.EndObject();

    // 写入文件
    std::ofstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR("[SceneSerializer] 无法写入文件: %s", filepath.c_str());
        return false;
    }
    file << w.GetString();
    file.close();

    LOG_INFO("[SceneSerializer] 场景已保存: %s (%u 个实体)", filepath.c_str(), world.GetEntityCount());
    return true;
}

// ═══════════════════════════════════════════════════════════════
// 加载
// ═══════════════════════════════════════════════════════════════

// 辅助：跳过对象中未知的 key-value
static void SkipValue(JsonParser& p) {
    auto t = p.Next();
    switch (t.Type) {
        case TokenType::LBrace: {
            while (p.Peek().Type != TokenType::RBrace) {
                p.Next(); // key
                p.Expect(TokenType::Colon);
                SkipValue(p);
                if (p.Peek().Type == TokenType::Comma) p.Next();
            }
            p.Expect(TokenType::RBrace);
            break;
        }
        case TokenType::LBracket: {
            while (p.Peek().Type != TokenType::RBracket) {
                SkipValue(p);
                if (p.Peek().Type == TokenType::Comma) p.Next();
            }
            p.Expect(TokenType::RBracket);
            break;
        }
        default: break; // string, number, bool, null 已消费
    }
}

// 辅助：读取对象的 key-value 对
static void ReadObjectFields(JsonParser& p, std::function<void(const std::string& key)> handler) {
    p.Expect(TokenType::LBrace);
    while (p.Peek().Type != TokenType::RBrace) {
        std::string key = p.ExpectStr();
        p.Expect(TokenType::Colon);
        handler(key);
        if (p.Peek().Type == TokenType::Comma) p.Next();
    }
    p.Expect(TokenType::RBrace);
}

Ref<Scene> SceneSerializer::Load(const std::string& filepath) {
    // 读取文件
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR("[SceneSerializer] 无法读取文件: %s", filepath.c_str());
        return nullptr;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    JsonParser p(ss.str());
    auto scene = std::make_shared<Scene>();
    auto& world = scene->GetWorld();

    // 添加默认系统
    world.AddSystem<MovementSystem>();
    world.AddSystem<LifetimeSystem>();

    ReadObjectFields(p, [&](const std::string& key) {
        if (key == "name") {
            scene->SetName(p.ExpectStr());
        }
        else if (key == "directionalLight") {
            auto& dl = scene->GetDirLight();
            ReadObjectFields(p, [&](const std::string& k) {
                if (k == "direction")  dl.Direction = p.ReadVec3();
                else if (k == "color") dl.Color = p.ReadVec3();
                else if (k == "intensity") dl.Intensity = (f32)p.ExpectNum();
                else SkipValue(p);
            });
        }
        else if (key == "pointLights") {
            p.Expect(TokenType::LBracket);
            while (p.Peek().Type != TokenType::RBracket) {
                auto& pl = scene->AddPointLight();
                ReadObjectFields(p, [&](const std::string& k) {
                    if (k == "position")      pl.Position = p.ReadVec3();
                    else if (k == "color")    pl.Color = p.ReadVec3();
                    else if (k == "intensity")  pl.Intensity = (f32)p.ExpectNum();
                    else if (k == "constant")   pl.Constant = (f32)p.ExpectNum();
                    else if (k == "linear")     pl.Linear = (f32)p.ExpectNum();
                    else if (k == "quadratic")  pl.Quadratic = (f32)p.ExpectNum();
                    else SkipValue(p);
                });
                if (p.Peek().Type == TokenType::Comma) p.Next();
            }
            p.Expect(TokenType::RBracket);
        }
        else if (key == "spotLights") {
            p.Expect(TokenType::LBracket);
            while (p.Peek().Type != TokenType::RBracket) {
                auto& sl = scene->AddSpotLight();
                ReadObjectFields(p, [&](const std::string& k) {
                    if (k == "position")       sl.Position = p.ReadVec3();
                    else if (k == "direction") sl.Direction = p.ReadVec3();
                    else if (k == "color")     sl.Color = p.ReadVec3();
                    else if (k == "intensity")   sl.Intensity = (f32)p.ExpectNum();
                    else if (k == "innerCutoff") sl.InnerCutoff = (f32)p.ExpectNum();
                    else if (k == "outerCutoff") sl.OuterCutoff = (f32)p.ExpectNum();
                    else if (k == "constant")    sl.Constant = (f32)p.ExpectNum();
                    else if (k == "linear")      sl.Linear = (f32)p.ExpectNum();
                    else if (k == "quadratic")   sl.Quadratic = (f32)p.ExpectNum();
                    else SkipValue(p);
                });
                if (p.Peek().Type == TokenType::Comma) p.Next();
            }
            p.Expect(TokenType::RBracket);
        }
        else if (key == "entities") {
            p.Expect(TokenType::LBracket);
            while (p.Peek().Type != TokenType::RBracket) {
                // 先创建实体
                std::string entityName = "Entity";
                // 两遍扫描太复杂，先创建再改名
                Entity entity = scene->CreateEntity();

                ReadObjectFields(p, [&](const std::string& k) {
                    if (k == "id") {
                        p.ExpectNum(); // 跳过旧 ID（创建新 ID）
                    }
                    else if (k == "tag") {
                        ReadObjectFields(p, [&](const std::string& tk) {
                            if (tk == "name") {
                                auto* tag = world.GetComponent<TagComponent>(entity);
                                if (tag) tag->Name = p.ExpectStr();
                            } else SkipValue(p);
                        });
                    }
                    else if (k == "transform") {
                        auto& tr = world.AddComponent<TransformComponent>(entity);
                        ReadObjectFields(p, [&](const std::string& tk) {
                            if (tk == "x") tr.X = (f32)p.ExpectNum();
                            else if (tk == "y") tr.Y = (f32)p.ExpectNum();
                            else if (tk == "z") tr.Z = (f32)p.ExpectNum();
                            else if (tk == "rotX") tr.RotX = (f32)p.ExpectNum();
                            else if (tk == "rotY") tr.RotY = (f32)p.ExpectNum();
                            else if (tk == "rotZ") tr.RotZ = (f32)p.ExpectNum();
                            else if (tk == "scaleX") tr.ScaleX = (f32)p.ExpectNum();
                            else if (tk == "scaleY") tr.ScaleY = (f32)p.ExpectNum();
                            else if (tk == "scaleZ") tr.ScaleZ = (f32)p.ExpectNum();
                            else SkipValue(p);
                        });
                    }
                    else if (k == "render") {
                        auto& rc = world.AddComponent<RenderComponent>(entity);
                        ReadObjectFields(p, [&](const std::string& rk) {
                            if (rk == "meshType") rc.MeshType = p.ExpectStr();
                            else if (rk == "objPath") rc.ObjPath = p.ExpectStr();
                            else if (rk == "colorR") rc.ColorR = (f32)p.ExpectNum();
                            else if (rk == "colorG") rc.ColorG = (f32)p.ExpectNum();
                            else if (rk == "colorB") rc.ColorB = (f32)p.ExpectNum();
                            else if (rk == "shininess") rc.Shininess = (f32)p.ExpectNum();
                            else SkipValue(p);
                        });
                    }
                    else if (k == "material") {
                        auto& mat = world.AddComponent<MaterialComponent>(entity);
                        ReadObjectFields(p, [&](const std::string& mk) {
                            if (mk == "diffuseR") mat.DiffuseR = (f32)p.ExpectNum();
                            else if (mk == "diffuseG") mat.DiffuseG = (f32)p.ExpectNum();
                            else if (mk == "diffuseB") mat.DiffuseB = (f32)p.ExpectNum();
                            else if (mk == "specularR") mat.SpecularR = (f32)p.ExpectNum();
                            else if (mk == "specularG") mat.SpecularG = (f32)p.ExpectNum();
                            else if (mk == "specularB") mat.SpecularB = (f32)p.ExpectNum();
                            else if (mk == "shininess") mat.Shininess = (f32)p.ExpectNum();
                            else if (mk == "roughness") mat.Roughness = (f32)p.ExpectNum();
                            else if (mk == "metallic") mat.Metallic = (f32)p.ExpectNum();
                            else if (mk == "textureName") mat.TextureName = p.ExpectStr();
                            else if (mk == "normalMapName") mat.NormalMapName = p.ExpectStr();
                            else if (mk == "emissive") mat.Emissive = p.ExpectBool();
                            else if (mk == "emissiveR") mat.EmissiveR = (f32)p.ExpectNum();
                            else if (mk == "emissiveG") mat.EmissiveG = (f32)p.ExpectNum();
                            else if (mk == "emissiveB") mat.EmissiveB = (f32)p.ExpectNum();
                            else if (mk == "emissiveIntensity") mat.EmissiveIntensity = (f32)p.ExpectNum();
                            else SkipValue(p);
                        });
                    }
                    else if (k == "health") {
                        auto& hp = world.AddComponent<HealthComponent>(entity);
                        ReadObjectFields(p, [&](const std::string& hk) {
                            if (hk == "current") hp.Current = (f32)p.ExpectNum();
                            else if (hk == "max") hp.Max = (f32)p.ExpectNum();
                            else SkipValue(p);
                        });
                    }
                    else if (k == "velocity") {
                        auto& vel = world.AddComponent<VelocityComponent>(entity);
                        ReadObjectFields(p, [&](const std::string& vk) {
                            if (vk == "vx") vel.VX = (f32)p.ExpectNum();
                            else if (vk == "vy") vel.VY = (f32)p.ExpectNum();
                            else if (vk == "vz") vel.VZ = (f32)p.ExpectNum();
                            else SkipValue(p);
                        });
                    }
                    else if (k == "ai") {
                        auto& ai = world.AddComponent<AIComponent>(entity);
                        ReadObjectFields(p, [&](const std::string& ak) {
                            if (ak == "scriptModule") ai.ScriptModule = p.ExpectStr();
                            else if (ak == "state") ai.State = p.ExpectStr();
                            else if (ak == "detectRange") ai.DetectRange = (f32)p.ExpectNum();
                            else if (ak == "attackRange") ai.AttackRange = (f32)p.ExpectNum();
                            else SkipValue(p);
                        });
                    }
                    else if (k == "lifetime") {
                        auto& lt = world.AddComponent<LifetimeComponent>(entity);
                        ReadObjectFields(p, [&](const std::string& lk) {
                            if (lk == "timeRemaining") lt.TimeRemaining = (f32)p.ExpectNum();
                            else SkipValue(p);
                        });
                    }
                    else {
                        SkipValue(p); // 未知组件，跳过
                    }
                });

                if (p.Peek().Type == TokenType::Comma) p.Next();
            }
            p.Expect(TokenType::RBracket);
        }
        else {
            SkipValue(p); // 未知顶级字段
        }
    });

    LOG_INFO("[SceneSerializer] 场景已加载: %s (%u 个实体, %zu 点光, %zu 聚光)",
             scene->GetName().c_str(), scene->GetEntityCount(),
             scene->GetPointLights().size(), scene->GetSpotLights().size());

    return scene;
}

} // namespace Engine
