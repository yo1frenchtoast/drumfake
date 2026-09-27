// Desktop replacement for avrlib/resources_manager.h: grids/resources.h only
// names these templates in a typedef, which DrumFreak never uses.
#ifndef DRUMFREAK_SHIMS_AVRLIB_RESOURCES_MANAGER_H_
#define DRUMFREAK_SHIMS_AVRLIB_RESOURCES_MANAGER_H_

namespace avrlib {

template<const prog_char* const* strings, const prog_uint16_t* const* lookup_tables>
struct ResourcesTables { };

template<typename ResourceId, typename Tables>
class ResourcesManager { };

}  // namespace avrlib

#endif  // DRUMFREAK_SHIMS_AVRLIB_RESOURCES_MANAGER_H_
