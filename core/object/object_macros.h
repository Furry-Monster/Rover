#pragma once

#define ROVER_CLASS(m_class, m_parent)                                          \
public:                                                                         \
    static rover::StringName get_class_name_static() { return #m_class; }       \
    static rover::StringName get_parent_class_name_static() { return #m_parent; } \
    rover::StringName get_class_name() const override { return #m_class; }      \
    bool is_class(const rover::StringName& p_name) const override {             \
        if (p_name == #m_class) return true;                                    \
        return m_parent::is_class(p_name);                                      \
    }                                                                           \
private:
