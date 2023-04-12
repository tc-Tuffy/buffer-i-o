#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

//对文件的抽象
struct File {        
  int fd;
  std::string filename;
};
// 文件的析构函数
struct FileClear {
  void operator()(File* f) {
    if (close(f->fd) != 0) {   // 关闭文件
      perror("close");
    }
    if (remove(f->filename.c_str()) != 0) {   //删除文件
      perror("remove");
    }
  }
};
//获取当前时间 精确到秒s
uint64_t GetCurrentTime () {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return tv.tv_sec;
}
int Write(int fd, const char* buf, size_t len) {
  ssize_t ret;
  while (len != 0 && (ret = write(fd, buf, len)) != 0) {
    if (ret == -1) {
      if (errno == EINTR) {
        continue;
      }
      perror("write");
      return -1;
    }
    len -= ret;
    buf += ret;
  }
  return 0;
}
int WriteFile(int fd, int unit, uint64_t fsize) {
  std::vector<char> buf(unit);
  for (auto& c : buf) {
    c = rand() % 256;
  }
  for (off_t offset = 0; offset < fsize; offset += unit) {
    size_t n = std::min((size_t)(fsize - offset), (size_t)unit);
    int ret = Write(fd, buf.data(), n);
    if (ret != 0) {
      return -1;
    }
  }
  return 0;
}
int InitFile(int fd, uint64_t fsize) {
  int ret = posix_fallocate(fd, 0, fsize);
  if (ret != 0) {
    perror("posix_fallocate");
    return -1;
  }
  return WriteFile(fd, 32 * 4096, fsize);
}
void GetReadAndWriteBytes(uint64_t& read_bytes, uint64_t& write_bytes) {
  pid_t pid = getpid();
  std::string proc_io = "/proc/" + std::to_string(pid) + "/io";
  std::ifstream ifs(proc_io);
  std::string s;
  while (std::getline(ifs, s)) {
    std::cout << s << std::endl;
    if (s.find("read_bytes: ") == 0) {
      auto pos = s.find(":") + 1;
      read_bytes = std::stoul(s.substr(pos));
    } else if (s.find("write_bytes: ") == 0) {
      auto pos = s.find(":") + 1;
      write_bytes = std::stoul(s.substr(pos));
    }
  }
}
int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout<< "input argc less than two"<<std::endl;
    return -1;
  }
  int unit = std::stoi(argv[1]);
  std::string fname = "/home/tuffy/projects/disk_io/" + std::to_string(GetCurrentTime()) + ".tmp";
  int fd = open(fname.c_str(), O_WRONLY | O_CREAT, 0666);
  if (fd < 0) {
    perror("open file error");
    return -1;
  }
  File f{fd, fname};
  std::unique_ptr<File, FileClear> defer_close(&f);

  constexpr uint64_t fsize = 128 * 1024 * 1024;
  int ret = InitFile(fd, fsize);
  if (ret != 0) {
    return -1;
  }
  off_t offset = lseek(fd, 0, SEEK_SET);    //文件读写偏移量设置为0
  assert(offset == 0);
  std::string evict_cache = "vmtouch -e " + fname;  //将文件从内存中驱逐
  system(evict_cache.data());
  uint64_t read_bytes0 = 0;
  uint64_t write_bytes0 = 0;
  ret = WriteFile(fd, unit, fsize);
  if (ret != 0) {
    return -1;
  }
  uint64_t read_bytes1 = 0;
  uint64_t write_bytes1 = 0;
  GetReadAndWriteBytes(read_bytes1, write_bytes1);
  printf("file_size %lu write_unit %d write_bytes %lu read_bytes %lu\n", fsize,
         unit, write_bytes1 - write_bytes0, read_bytes1 - read_bytes0);
  return 0;
}