// Copyright 2026 The Reode Authors
//
// geode::JsonExpectedValue: a JSON validator that walks a value, records the
// first error with the path where it happened, and never throws.

#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Geode/utils/JsonValidation.hpp"
#include "fmt/format.h"
#include "host.h"
#include "matjson.hpp"

namespace geode {
namespace {

struct SharedState {
  std::optional<std::string> error;
};

}  // namespace

class JsonExpectedValue::Impl {
 public:
  std::shared_ptr<SharedState> state = std::make_shared<SharedState>();
  // Keeps the root value alive for every child that points into it.
  std::shared_ptr<matjson::Value> root;
  matjson::Value* value = &missing;
  // Stands in for keys and indices that do not exist.
  matjson::Value missing;
  std::string key;
  std::set<std::string> known_keys;
};

JsonExpectedValue::JsonExpectedValue() : m_impl(std::make_unique<Impl>()) {}

JsonExpectedValue::JsonExpectedValue(Impl* from, matjson::Value& scope,
                                     std::string key)
    : m_impl(std::make_unique<Impl>()) {
  m_impl->state = from->state;
  m_impl->root = from->root;
  m_impl->value = &scope;
  m_impl->key = std::move(key);
}

JsonExpectedValue::JsonExpectedValue(matjson::Value value,
                                     std::string rootScopeName)
    : m_impl(std::make_unique<Impl>()) {
  m_impl->root = std::make_shared<matjson::Value>(std::move(value));
  m_impl->value = m_impl->root.get();
  m_impl->key = std::move(rootScopeName);
}

JsonExpectedValue::~JsonExpectedValue() = default;
JsonExpectedValue::JsonExpectedValue(JsonExpectedValue&&) = default;
JsonExpectedValue& JsonExpectedValue::operator=(JsonExpectedValue&&) = default;

const char* JsonExpectedValue::matJsonTypeToString(matjson::Type ty) {
  switch (ty) {
    case matjson::Type::Null:
      return "null";
    case matjson::Type::Bool:
      return "bool";
    case matjson::Type::Number:
      return "number";
    case matjson::Type::String:
      return "string";
    case matjson::Type::Array:
      return "array";
    case matjson::Type::Object:
      return "object";
  }
  return "unknown";
}

bool JsonExpectedValue::hasError() const {
  return m_impl->state->error.has_value();
}

void JsonExpectedValue::setError(std::string_view error) {
  if (hasError()) return;
  m_impl->state->error = fmt::format("{}: {}", m_impl->key, error);
}

matjson::Value const& JsonExpectedValue::getJSONRef() const {
  return *m_impl->value;
}

matjson::Value JsonExpectedValue::json() const { return *m_impl->value; }

matjson::Value JsonExpectedValue::takeJson() {
  return std::move(*m_impl->value);
}

std::string JsonExpectedValue::key() const { return m_impl->key; }

bool JsonExpectedValue::is(matjson::Type type) const {
  return !hasError() && m_impl->value->type() == type;
}
bool JsonExpectedValue::isNull() const { return is(matjson::Type::Null); }
bool JsonExpectedValue::isBool() const { return is(matjson::Type::Bool); }
bool JsonExpectedValue::isNumber() const { return is(matjson::Type::Number); }
bool JsonExpectedValue::isString() const { return is(matjson::Type::String); }
bool JsonExpectedValue::isArray() const { return is(matjson::Type::Array); }
bool JsonExpectedValue::isObject() const { return is(matjson::Type::Object); }

JsonExpectedValue& JsonExpectedValue::assertIs(matjson::Type type) {
  if (!hasError() && m_impl->value->type() != type) {
    setError(fmt::format("expected {}, got {}", matJsonTypeToString(type),
                         matJsonTypeToString(m_impl->value->type())));
  }
  return *this;
}

JsonExpectedValue& JsonExpectedValue::assertIs(
    std::initializer_list<matjson::Type> types) {
  if (hasError()) return *this;
  for (matjson::Type type : types) {
    if (m_impl->value->type() == type) return *this;
  }
  std::string expected;
  for (matjson::Type type : types) {
    if (!expected.empty()) expected += " or ";
    expected += matJsonTypeToString(type);
  }
  setError(fmt::format("expected {}, got {}", expected,
                       matJsonTypeToString(m_impl->value->type())));
  return *this;
}

JsonExpectedValue& JsonExpectedValue::assertIsNull() {
  return assertIs(matjson::Type::Null);
}
JsonExpectedValue& JsonExpectedValue::assertIsBool() {
  return assertIs(matjson::Type::Bool);
}
JsonExpectedValue& JsonExpectedValue::assertIsNumber() {
  return assertIs(matjson::Type::Number);
}
JsonExpectedValue& JsonExpectedValue::assertIsString() {
  return assertIs(matjson::Type::String);
}
JsonExpectedValue& JsonExpectedValue::assertIsArray() {
  return assertIs(matjson::Type::Array);
}
JsonExpectedValue& JsonExpectedValue::assertIsObject() {
  return assertIs(matjson::Type::Object);
}

JsonExpectedValue JsonExpectedValue::has(std::string key) {
  std::string path = m_impl->key + "." + key;
  JsonExpectedValue missing(m_impl.get(), m_impl->missing, path);
  missing.m_impl->value = &missing.m_impl->missing;
  if (hasError()) return missing;
  if (!m_impl->value->isObject()) {
    assertIsObject();
    return missing;
  }
  if (!m_impl->value->contains(key)) return missing;
  m_impl->known_keys.insert(key);
  return JsonExpectedValue(m_impl.get(), (*m_impl->value)[key],
                           std::move(path));
}

JsonExpectedValue JsonExpectedValue::hasNullable(std::string key) {
  return has(std::move(key));
}

JsonExpectedValue JsonExpectedValue::needs(std::string key) {
  if (!hasError() && m_impl->value->isObject() &&
      !m_impl->value->contains(key)) {
    setError(fmt::format("missing required key \"{}\"", key));
  }
  return has(std::move(key));
}

std::vector<std::pair<std::string, JsonExpectedValue>>
JsonExpectedValue::properties() {
  std::vector<std::pair<std::string, JsonExpectedValue>> result;
  if (hasError() || !assertIsObject()) return result;
  for (matjson::Value& member : *m_impl->value) {
    std::string name = member.getKey().value_or("");
    m_impl->known_keys.insert(name);
    result.emplace_back(name, JsonExpectedValue(m_impl.get(), member,
                                                m_impl->key + "." + name));
  }
  return result;
}

void JsonExpectedValue::checkUnknownKeys() {
  if (hasError() || !m_impl->value->isObject()) return;
  for (const matjson::Value& member : *m_impl->value) {
    std::string name = member.getKey().value_or("");
    if (!m_impl->known_keys.contains(name)) {
      reode::geode_shim::Log(
          nullptr, kReodeLogDebug,
          fmt::format("[Geode.dll] {} has unknown key \"{}\"", m_impl->key,
                      name));
    }
  }
}

size_t JsonExpectedValue::length() {
  if (hasError() || !assertIsArray()) return 0;
  return m_impl->value->size();
}

JsonExpectedValue JsonExpectedValue::at(size_t index) {
  std::string path = fmt::format("{}[{}]", m_impl->key, index);
  if (hasError() || !assertIsArray() || index >= m_impl->value->size()) {
    if (!hasError()) setError(fmt::format("index {} out of range", index));
    JsonExpectedValue missing(m_impl.get(), m_impl->missing, path);
    missing.m_impl->value = &missing.m_impl->missing;
    return missing;
  }
  return JsonExpectedValue(m_impl.get(), (*m_impl->value)[index],
                           std::move(path));
}

std::vector<JsonExpectedValue> JsonExpectedValue::items() {
  std::vector<JsonExpectedValue> result;
  if (hasError() || !assertIsArray()) return result;
  for (size_t i = 0; i < m_impl->value->size(); ++i) {
    // The constructor is private, so the element is built here.
    result.push_back(JsonExpectedValue(m_impl.get(), (*m_impl->value)[i],
                                       fmt::format("{}[{}]", m_impl->key, i)));
  }
  return result;
}

JsonExpectedValue::operator bool() const { return !hasError(); }

Result<> JsonExpectedValue::ok() {
  if (hasError()) return Err(*m_impl->state->error);
  return Ok();
}

JsonExpectedValue checkJson(matjson::Value json, std::string rootScopeName) {
  return JsonExpectedValue(std::move(json), std::move(rootScopeName));
}

}  // namespace geode
