add_rules("mode.debug", "mode.release")

target("cob")
    set_kind("binary")
    add_files("src/core/**.cpp")
    add_files("src/*.cpp")
    add_includedirs("src/heads") 