1. 安装 gcc (可选) https://gist.github.com/nchaigne/ad06bc867f911a3c0d32939f1e930a11
2. 安装 texlive (是否需要？)
   1. wget https://mirrors.tuna.tsinghua.edu.cn/CTAN/systems/texlive/tlnet/install-tl-unix.tar.gz
   2. ./install-tl -repository https://mirrors.tuna.tsinghua.edu.cn/CTAN/systems/texlive/tlnet
   3. 选择 scheme-small
3. 安装 protobuf
   1. wget https://github.com/protocolbuffers/protobuf/releases/download/v3.12.3/protoc-3.12.3-linux-x86_64.zip
   2. unzip protocxxx
   3. cp bin/protoc /usr/bin
   4. cp -r include/google /usr/include
   5. wget https://github.com/protocolbuffers/protobuf/releases/download/v3.12.3/protobuf-cpp-3.12.3.tar.gz
   6. tar xzvf protobuf-cppxxx
   7. cd protobuf-cpp...
   8. ./configure
   9. make -j
   10. make check
   11. make install
   12. ldconfig
   13. https://github.com/protocolbuffers/protobuf/blob/master/src/README.md
   14. cp /usr/local/lib/libproto* /lib64/
4. 安装 python 包
   1. yum install python3-pip
   2. pip3 install numpy matplotlib jinja2 requests protobuf