# Copies the optional sysmodules built beside the app (sysmodule/<name>/out/
# playguard-<name>.nsp) into the romfs, as sysmodules/<name>/exefs.nsp with a
# version.txt (app version, commit, SHA-256), for Tools › Optional modules
# (source/util/modules.hpp). A module not built (the desktop build, a
# checkout without devkitPro) is left out: the screen then says so.
#   cmake -DSOURCE_DIR=… -DDEST=… -DAPP_VERSION=… -DAPP_COMMIT=… -P bundle_sysmodules.cmake
foreach (name rescue agent)
    set(src "${SOURCE_DIR}/sysmodule/${name}/out/playguard-${name}.nsp")
    if (EXISTS "${src}")
        file(MAKE_DIRECTORY "${DEST}/${name}")
        configure_file("${src}" "${DEST}/${name}/exefs.nsp" COPYONLY)
        file(SHA256 "${src}" hash)
        file(WRITE "${DEST}/${name}/version.txt" "version=${APP_VERSION}\ncommit=${APP_COMMIT}\nsha256=${hash}\n")
        message(STATUS "romfs: sysmodule ${name} ${hash}")
    endif ()
endforeach ()
