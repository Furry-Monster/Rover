#include "core/os/dir_access.h"

namespace rover
{

    DirAccess::Factory& DirAccess::factory_slot()
    {
        static Factory slot = nullptr;
        return slot;
    }

    void DirAccess::set_factory(Factory factory)
    {
        factory_slot() = factory;
    }

    std::unique_ptr<DirAccess> DirAccess::create()
    {
        auto factory = factory_slot();
        if (!factory)
        {
            return nullptr;
        }
        return factory();
    }

} // namespace rover
