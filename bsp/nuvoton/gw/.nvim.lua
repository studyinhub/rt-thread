require("lspconfig").clangd.setup({
	cmd = {
		"clangd",
		"--query-driver=/Users/yangxiyuan/Projects/rtt-projects/toolchains/gcc-arm-none-eabi-5_4-2016q3/", -- 如果需要指定特定的编译器路径
	},
	filetypes = { "c", "cpp", "objc", "objcpp", "cuda", "proto" },
})
