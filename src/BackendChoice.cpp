#include <cellui/BackendChoice.h>

#include <cstdlib>

namespace cellui {

const char * backendName(BackendKind kind) {
  switch (kind) {
    case BackendKind::AUTO: return "auto";
    case BackendKind::TERMINAL: return "terminal";
    case BackendKind::WINDOW: return "window";
  }
  return "";
}

bool parseBackendKind(std::string_view name, BackendKind & kind) {
  for (BackendKind k : {BackendKind::AUTO, BackendKind::TERMINAL, BackendKind::WINDOW}) {
    if (name == backendName(k)) {
      kind = k;
      return true;
    }
  }
  return false;
}

BackendEnvironment probeBackendEnvironment() {
  BackendEnvironment environment;
#if defined(CELLUI_HAS_GTK) || defined(CELLUI_HAS_WIN32)
  environment.window_built = true;
#endif
#if defined(_WIN32)
  environment.display = true;
#else
  auto set = [](const char * name) {
    const char * value = std::getenv(name);
    return value && *value;
  };
  environment.display = set("WAYLAND_DISPLAY") || set("DISPLAY");
#endif
  return environment;
}

std::vector<BackendKind> backendCandidates(BackendKind requested, const BackendEnvironment & environment) {
  if (requested != BackendKind::AUTO) return {requested};
  std::vector<BackendKind> candidates;
  if (environment.window_built && environment.display) candidates.push_back(BackendKind::WINDOW);
  candidates.push_back(BackendKind::TERMINAL);
  return candidates;
}

}  // namespace cellui
