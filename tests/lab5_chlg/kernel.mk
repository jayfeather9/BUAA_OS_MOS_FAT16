init-envs := chlg_check /fs_serv /fs_fatserv
fat-files  += $(wildcard $(test_dir)/rootfat/*)
fs-files  := $(wildcard $(test_dir)/rootfs/*)