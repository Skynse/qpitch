#include <clap/entry.h>
#include <clap/factory/plugin-factory.h>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>

int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    auto *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module)
    {
        std::fprintf(stderr, "%s\n", dlerror());
        return 2;
    }
    auto *entry = static_cast<const clap_plugin_entry_t *>(dlsym(module, "clap_entry"));
    if (!entry || !entry->init(argv[1]))
        return 3;
    auto *factory = static_cast<const clap_plugin_factory_t *>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory || factory->get_plugin_count(factory) == 0)
        return 4;
    auto *descriptor = factory->get_plugin_descriptor(factory, 0);
    if (!descriptor || std::strcmp(descriptor->id, "com.qpitch.autotune") != 0 ||
        std::strcmp(descriptor->version, "2.0.0") != 0)
        return 5;
    std::printf("PASS CLAP load and metadata: %s %s\n", descriptor->name, descriptor->version);
    entry->deinit();
    dlclose(module);
    return 0;
}
