# 编译gcc作为交叉编译器<a id="appendix2"></a>

注意，gcc需要从源码构建，并配置为交叉编译器，即目标平台为`i686-elf`。你可以使用本项目提供的[自动化脚本](slides/c0-workspace/gcc-build.sh)，这将会涵盖gcc和binutils源码的下载，配置和编译（没什么时间去打磨脚本，目前只知道在笔者的Ubuntu系统上可以运行）。

**推荐**手动编译。以下编译步骤搬运自：<https://wiki.osdev.org/GCC_Cross-Compiler>

**首先安装构建依赖项：**

```bash
sudo apt update &&\
     apt install -y \
        build-essential \
        bison\
        flex\
        libgmp3-dev\
        libmpc-dev\
        libmpfr-dev\
        texinfo
```

**开始编译：**

1. 获取[gcc](https://ftp.gnu.org/gnu/gcc/)和[binutils](https://ftp.gnu.org/gnu/binutils)源码
2. 解压，并在同级目录为gcc和binutil新建专门的build文件夹

现在假设你的目录结构如下：

```
+ folder
  + gcc-src
  + binutils-src
  + gcc-build
  + binutils-build
```

3. 确定gcc和binutil安装的位置，并设置环境变量：`export PREFIX=<安装路径>` 然后设置PATH： `export PATH="$PREFIX/bin:$PATH"`
4. 设置目标平台：`export TARGET=i686-elf`
5. 进入`binutils-build`进行配置

```bash
../binutils-src/configure --target="$TARGET" --prefix="$PREFIX" \
 --with-sysroot --disable-nls --disable-werror
```

然后 `make && make install`

6. 确保上述的`binutils`已经正常安装：执行：`which i686-elf-as`，应该会给出一个位于你安装目录下的路径。
6. 进入`gcc-build`进行配置

```bash
../gcc-src/configure --target="$TARGET" --prefix="$PREFIX" \
 --disable-nls --enable-languages=c,c++ --without-headers
```

然后编译安装（取决性能，大约10~20分钟）：

```bash
make all-gcc &&\
 make all-target-libgcc &&\
 make install-gcc &&\
 make install-target-libgcc
```

8. 验证安装：执行`i686-elf-gcc -dumpmachine`，输出应该为：`i686-elf`

**将新编译好的GCC永久添加到`PATH`环境变量**

虽然这是一个常识性的操作，但考虑到许多人都会忽略这一个额外的步骤，在这里特此做出提示。

要想实现这一点，只需要在shell的配置文件的末尾添加：`export PATH="<上述的安装路径>/bin:$PATH"`。

这个配置文件是取决于你使用的shell，如zsh就是`${HOME}/.zshrc`，bash则是`${HOME}/.bashrc`；或者你嫌麻烦的，懒得区分，你也可以直接修改全局的`/etc/profile`文件，一劳永逸（但不推荐这样做）。

至于其他的情况，由于这个步骤其实在网上是随处可查的，所以就不在这里赘述了。
