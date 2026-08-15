//
// Copyright (c) ALRIGROUP and its affiliates.
// C++ Bindings for ARWN (Alri Real-Time Web Node)
//

#ifndef ARWN_HPP
#define ARWN_HPP

#include "arwn.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace arwn {

class App {
public:
    explicit App(const std::string &name)
        : app_(arwn_app_new(name.c_str()), arwn_app_free) {}

    bool load_config(const std::string &path) {
        if (!app_) return false;
        return arwn_config_load(app_.get(), path.c_str()) == 0;
    }

    bool parse_config(const std::string &buffer) {
        if (!app_) return false;
        return arwn_config_parse_buffer(app_.get(), buffer.data(), buffer.size()) == 0;
    }

    std::string get_name() const {
        return app_ ? arwn_app_name(app_.get()) : "";
    }

    std::string get_root() const {
        return app_ ? arwn_app_root(app_.get()) : "";
    }

    std::string get_string(const std::string &section, const std::string &key, const std::string &def = "") const {
        return app_ ? arwn_config_get(app_.get(), section.c_str(), key.c_str(), def.c_str()) : def;
    }

    int get_int(const std::string &section, const std::string &key, int def = 0) const {
        return app_ ? arwn_config_get_int(app_.get(), section.c_str(), key.c_str(), def) : def;
    }

    bool get_bool(const std::string &section, const std::string &key, bool def = false) const {
        return app_ ? (arwn_config_get_bool(app_.get(), section.c_str(), key.c_str(), def ? 1 : 0) != 0) : def;
    }

    int unit_count() const {
        return app_ ? arwn_config_unit_count(app_.get()) : 0;
    }

    const arwn_unit_t* unit(int idx) const {
        return app_ ? arwn_config_unit(app_.get(), idx) : nullptr;
    }

    int build() {
        return app_ ? arwn_builder_execute(app_.get()) : -1;
    }

    int mount() {
        return app_ ? arwn_mount(app_.get()) : -1;
    }

    std::string last_error() const {
        return app_ ? arwn_config_last_error(app_.get()) : "null app";
    }

    arwn_app_t *raw() const { return app_.get(); }

private:
    std::unique_ptr<arwn_app_t, void(*)(arwn_app_t*)> app_;
};

} // namespace arwn

#endif // ARWN_HPP
