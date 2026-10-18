#!/usr/bin/env bash
# =====================================================================
#  纹理映射实验 —— Ubuntu 一键安装 / 编译脚本
#  适用：Ubuntu 22.04 及以上（含平板上的 proot / Termux / Andronix / UserLAnd 等模拟器，x86_64 或 arm64）
#
#  用法（在本项目根目录执行）：
#     bash setup_ubuntu.sh            # 安装依赖 + 编译
#     bash setup_ubuntu.sh --mirror   # 先把 apt 换成中科大镜像（国内网络推荐）
#
#  完成后运行：
#     ./run.sh task1      # 任务1 Mipmap 走廊
#     ./run.sh task2      # 任务2 天空盒 + 反射球
#     ./run.sh viewer     # 任务3 3D 模型查看器（含 mipmap / 环境映射插件）
# =====================================================================
set -e
cd "$(dirname "$0")"

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

# ---------- 1. 可选：换国内镜像 ----------
if [ "$1" = "--mirror" ]; then
    echo ">>> 切换 apt 镜像为 mirrors.ustc.edu.cn"
    ARCH=$(dpkg --print-architecture)
    if [ "$ARCH" = "amd64" ] || [ "$ARCH" = "i386" ]; then REPO="ubuntu"; else REPO="ubuntu-ports"; fi
    for f in /etc/apt/sources.list /etc/apt/sources.list.d/ubuntu.sources; do
        [ -f "$f" ] || continue
        $SUDO cp -n "$f" "$f.bak" 2>/dev/null || true
        $SUDO sed -i -E "s#https?://[^ ]*(archive|security|ports)\.ubuntu\.com/ubuntu(-ports)?/?#http://mirrors.ustc.edu.cn/${REPO}/#g" "$f"
    done
fi

# ---------- 2. 安装依赖 ----------
echo ">>> 安装编译依赖（Qt6、CMake、Mesa 软件渲染、中文字体）"
$SUDO apt-get update
PKGS="build-essential cmake ninja-build pkg-config qt6-base-dev libgl1-mesa-dev libgl1-mesa-dri fonts-noto-cjk"
# 以下包名在不同 Ubuntu 版本里可能不存在或已合并，存在才装
for p in libqt6opengl6-dev qt6-qpa-plugins libxkbcommon-x11-0 libxcb-cursor0 mesa-utils; do
    if apt-cache show "$p" >/dev/null 2>&1; then PKGS="$PKGS $p"; fi
done
DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y --no-install-recommends $PKGS

# ---------- 3. 编译 ----------
echo ">>> 编译（Release）"
JOBS=$(nproc 2>/dev/null || echo 2)
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j"$JOBS"

# ---------- 4. 生成启动脚本 ----------
cat > run.sh <<'EOF'
#!/usr/bin/env bash
# 启动实验程序：./run.sh task1 | task2 | viewer
cd "$(dirname "$0")/build-linux/bin"
if [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
    echo "没有检测到图形界面（DISPLAY 为空）。"
    echo "请先打开 VNC / Termux:X11 桌面，并在该桌面的终端里运行；或手动 export DISPLAY=:0（或 :1）。"
    exit 1
fi
export QT_X11_NO_MITSHM=1             # proot 环境通常没有共享内存，关掉 MIT-SHM 避免黑屏
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-xcb}
# 平板模拟器里一般没有 GPU 驱动，Mesa 会自动退回 llvmpipe 软件渲染（支持 OpenGL 4.5，够用但较慢）
case "$1" in
    task1)  exec ./task1_mipmap ;;
    task2)  exec ./task2_envmap ;;
    viewer|"") exec ./viewer3d ;;
    *) echo "用法: ./run.sh task1 | task2 | viewer"; exit 1 ;;
esac
EOF
chmod +x run.sh

echo
echo "=============================================="
echo " 编译完成！程序位于 build-linux/bin/"
echo "   ./run.sh task1    任务1 Mipmap 走廊"
echo "   ./run.sh task2    任务2 天空盒 + 反射球"
echo "   ./run.sh viewer   任务3 3D 模型查看器"
echo "=============================================="
