# GGELUA Godot模块

GGELUA模块为Godot引擎提供了《大话西游2》和《梦幻西游》游戏资源文件的读取和解析功能。

## 🔥 重要说明

**此模块完全自包含，不依赖任何外部GGELUA文件夹！**

所有必要的核心功能都已集成到模块内部，您可以安全地删除外部GGELUA目录，模块仍然可以正常工作。

## 功能特性

- **TCP动画文件支持**: 读取和解析TCP格式的精灵动画文件
- **WDF资源包支持**: 访问WDF格式的资源包文件  
- **地图文件支持**: 加载M1.0和MAPX格式的地图文件
- **哈希工具**: 提供文件名哈希计算功能

## 安装

1. 将GGELUA模块放入Godot源码的`modules/ggelua/`目录
2. 重新编译Godot引擎：
   ```bash
   scons platform=windows target=editor
   ```
3. 编译完成后，新的类将在GDScript中可用

## 快速开始

### 加载TCP动画文件

```gdscript
# 创建TCP加载器
var tcp_loader = TCPLoader.new()

# 从文件加载
if tcp_loader.load_from_file("res://character.tcp") == OK:
    print("动画组数: ", tcp_loader.get_group_count())
    print("每组帧数: ", tcp_loader.get_frame_count())
    
    # 获取第一帧图像
    var frame = tcp_loader.get_frame(0)
    if frame:
        # 创建纹理并显示
        var texture = ImageTexture.create_from_image(frame)
        $Sprite2D.texture = texture
```

### 访问WDF资源包

```gdscript
# 打开WDF文件
var wdf = WDFArchive.new()
if wdf.open("res://resources.wdf") == OK:
    print("文件数量: ", wdf.get_file_count())
    
    # 通过哈希获取文件
    var hash = GGELUAHash.calculate_hash("shape/character/001.tcp")
    var tcp = wdf.get_tcp_file_by_hash(hash)
    if tcp:
        print("成功加载TCP文件")
```

### 加载游戏地图

```gdscript
# 打开地图文件
var map_loader = MapLoader.new()
if map_loader.open("res://map001.map") == OK:
    print("地图格式: ", map_loader.get_map_format())
    print("地图尺寸: ", map_loader.get_map_size())
    print("地图块数: ", map_loader.get_map_count())
    
    # 获取第一个地图块
    var tile_image = map_loader.get_map_tile(0)
    if tile_image:
        var texture = ImageTexture.create_from_image(tile_image)
        $Sprite2D.texture = texture
```

## API文档

详细的API文档请参考：
- [TCPLoader](doc_classes/TCPLoader.xml) - TCP动画文件加载器
- [WDFArchive](doc_classes/WDFArchive.xml) - WDF资源包访问器
- [MapLoader](doc_classes/MapLoader.xml) - 地图文件加载器
- [GGELUAHash](doc_classes/GGELUAHash.xml) - 哈希工具

## 示例项目

参考`examples/`目录中的示例项目：
- `tcp_viewer/` - TCP动画查看器
- `wdf_explorer/` - WDF资源包浏览器
- `map_viewer/` - 地图查看器

## 许可证

本模块基于MIT许可证发布，与Godot引擎保持一致。

## 贡献

欢迎提交Issue和Pull Request来改进此模块。

## 技术支持

如需技术支持，请在GitHub上创建Issue。