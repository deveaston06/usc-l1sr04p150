# Firmware Coding for USC-L1SR04D150 IoT Station

![screenshot](https://github.com/ironlungx/icons/blob/main/screenshot.png?raw=true)

## Overview

This firmware is for my capstone project and is my first project made with []() using [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html) and [Neovim (Lazyvim)](https://www.lazyvim.org/installation), but using [VSCode](https://code.visualstudio.com/) and [PlatformIO Extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide) works too.

---

## Setup

1. Configure Neovim LSP

Install the following Neovim plugins (using `lazy.nvim` or your preferred manager):

```lua
return {
  {
    "williamboman/mason.nvim",
    config = function()
      require("mason").setup({})
    end,
  },
  {
    "williamboman/mason-lspconfig.nvim",
    config = function()
      require("mason-lspconfig").setup({
        ensure_installed = { "clangd", "lua_ls" },
      })
    end,
  },
  {
    "neovim/nvim-lspconfig",
    config = function()
      local lspconfig = require("lspconfig")
      lspconfig.lua_ls.setup({})
      lspconfig.clangd.setup({
        cmd = { "clangd", "--background-index" },
      })
    end,
  },
}
```
2. Configure PlatformIO

```sh
cd usc-l1sr04d154
pio init --ide vim
```

## Developing

Make sure ESP32 is connected to the computer first before uploading using command `lsusb`. You can add libraries to the project by modifying `platformio.ini`.

1. Link up Neovim LSP

Make sure run the command too when every time you modify `platformio.ini`.

```sh
python3 conv.py
```

2. Compile, upload and monitor

```sh
pio run -t upload -t monitor
```

---

## Related Projects

* [nvim-platformio.lua](https://github.com/anurag3301/nvim-platformio.lua)

> Found a better way or improvement? Open a PR or issue!
