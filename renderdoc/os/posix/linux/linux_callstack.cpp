/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <map>
#include "common/common.h"
#include "common/formatting.h"
#include "os/os_specific.h"

class LinuxCallstack : public Callstack::Stackwalk
{
public:
  LinuxCallstack()
  {
    RDCEraseEl(frames);
    frame_count = 0;
    Collect();
  }
  LinuxCallstack(uint64_t *calls, size_t num)
  {
    Set(calls, num);
  }

  ~LinuxCallstack()
  {
  }

  void Set(uint64_t *calls, size_t num)
  {
    frame_count = num;
    for (size_t i = 0; i < frame_count; i++)
    {
      frames[i] = calls[i];
    }
  }

  size_t NumLevels() const
  {
    return frame_count;
  }
  const uint64_t *GetAddrs() const
  {
    return frames;
  }

private:
  LinuxCallstack(const Callstack::Stackwalk &other);

  void Collect();

  uint64_t frames[128];
  size_t frame_count;
};

struct backtrace_details
{
  uintptr_t addr2line_addr;
  rdcstr filename;
  rdcstr symbolname;
};

namespace Callstack
{
Stackwalk *Collect()
{
  return new LinuxCallstack();
}

Stackwalk *Create()
{
  return new LinuxCallstack(NULL, 0);
}

class LinuxResolver : public Callstack::StackResolver
{
public:
  LinuxResolver(std::map<uint64_t, Callstack::AddressDetails> const &rdc_cache) : m_rdc_cache(rdc_cache)
  {
  }

  Callstack::AddressDetails GetAddr(uint64_t addr)
  {
    return m_rdc_cache[addr];
  }

private:
  std::map<uint64_t, Callstack::AddressDetails> m_rdc_cache;
};

}; // namespace Callstack

#include "libc_malloc_debug_backtrace/backtrace.h"
#include <inttypes.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <string>

void Callstack::Init()
{
  backtrace_startup();
}

static std::map<uintptr_t, backtrace_details>
    g_backtrace_cache;

void LinuxCallstack::Collect()
{
  frame_count = backtrace_get(frames, ARRAY_COUNT(frames));
  for (size_t frame_num = 0; frame_num < frame_count; frame_num++)
  {
    uintptr_t frame = frames[frame_num];

    auto ret_pair = g_backtrace_cache.insert(std::pair<uintptr_t, backtrace_details>(frame, backtrace_details{}));
    if (ret_pair.second)
    {
      backtrace_details &ret = ret_pair.first->second;
      uintptr_t addr2line_addr;
      char filename[4096];
      char symbolname[4096];
      backtrace_resolve(frame, &addr2line_addr, filename, 4096, symbolname, 4096, NULL);
      ret.addr2line_addr = addr2line_addr;
      ret.filename = filename;
      ret.symbolname = symbolname;
    }
  }
}

bool Callstack::GetLoadedModules(byte *buf, size_t &size)
{
  size = 0;

  if (buf)
  {
    memcpy(buf, "LNUXCALL", 8);
  }
  size += 8;

  std::string db_str;

  for (auto const &r : g_backtrace_cache)
  {
    char buf[4096];
    snprintf(buf, sizeof(buf), "%" PRIxPTR " %" PRIxPTR " %s %s\n", r.first, r.second.addr2line_addr, r.second.filename.c_str(), r.second.symbolname.c_str());
    db_str += buf;
  }

  if (buf)
  {
    memcpy(buf + 8, db_str.data(), db_str.length());
  }
  size += db_str.length();

  return true;
}

static inline void execute_addr2line(char addr2line_result[4096], char const *filename, uintptr_t addr2line_addr);

Callstack::StackResolver *Callstack::MakeResolver(bool interactive, byte *moduleDB, size_t DBSize, RENDERDOC_ProgressCallback progress)
{
  // we look in the original locations for the files, we don't prompt if we can't
  // find the file, or the file doesn't have symbols (and we don't validate that
  // the file is the right version). A good option for doing this would be
  // http://github.com/mlabbe/nativefiledialog

  if (DBSize < 8 || memcmp(moduleDB, "LNUXCALL", 8))
  {
    RDCWARN("Can't load callstack resolve for this log. Possibly from another platform?");
    return NULL;
  }

  char const *db_start = reinterpret_cast<char const *>(reinterpret_cast<uintptr_t>(moduleDB) + 8);
  char const *db_end = reinterpret_cast<char const *>(reinterpret_cast<uintptr_t>(moduleDB) + DBSize);

  std::map<uint64_t, Callstack::AddressDetails> rdc_cache;

  char const *db_current = db_start;
  while (db_current < db_end)
  {

    char const *line = db_current;
    char const *line_end = strchr(line, '\n');
    if (line_end != NULL)
    {
      uintptr_t frame;
      uintptr_t addr2line_addr;
      std::string filename;
      std::string symbolname;
      int ret_sscanf;
      {
        char filename_cstr[4096];
        char symbolname_cstr[4096];
        ret_sscanf = sscanf(line, "%" SCNxPTR " %" SCNxPTR " %4096s %4096s", &frame, &addr2line_addr, filename_cstr, symbolname_cstr);

        filename = filename_cstr;
        symbolname = symbolname_cstr;
      }

      if (ret_sscanf == 4)
      {
        auto ret_pair = rdc_cache.insert(std::pair<uint64_t, Callstack::AddressDetails>(frame, Callstack::AddressDetails{}));
        if (ret_pair.second)
        {
          Callstack::AddressDetails &ret = ret_pair.first->second;
          
          char addr2line_result[4096];
          execute_addr2line(addr2line_result, filename.c_str(), addr2line_addr);

          char *line1 = NULL;
          char *line2_1 = NULL;
          char *line2_2 = NULL;
          {
            if (strlen(addr2line_result) > 0)
            {
              line1 = addr2line_result;
            }

            if (line1 != NULL)
            {
              line2_1 = strchr(line1, '\n');
              if (line2_1 != NULL)
              {
                (*line2_1) = '\0';
                ++line2_1;
              }
            }

            if (line2_1 != NULL)
            {
              line2_2 = strrchr(line2_1, ':');
              if (line2_2 != NULL)
              {
                (*line2_2) = '\0';
                ++line2_2;
              }
            }
          }

          if (line1 != NULL && strchr(line1, '?') == NULL)
          {
            ret.function = line1;
          }
          else
          {
            //char pasudo_function[] = {"ffffffffffffffff"};
            //snprintf(pasudo_function, sizeof(pasudo_function) / sizeof(char), "%" PRIxPTR, addr2line_addr);
            //ret.function = pasudo_function;
            ret.function = symbolname.c_str();
          }

          if (line2_1 != NULL && strchr(line2_1, '?') == NULL)
          {
            ret.filename = line2_1;
          }
          else
          {
            ret.filename = filename.c_str();
          }

          if (line2_2 != NULL && strchr(line2_2, '?') == NULL)
          {
            ret.line = atoll(line2_2);
          }
          else
          {
            ret.line = -1;
          }
        }
      }
      else
      {
        std::string line_cxx(line, line_end - line);
        RDCWARN("ModuleDB: can't resolve line: %s", line_cxx.c_str());
      }

      db_current = (line_end + 1);

      if (progress)
      {
        progress(RDCMIN(1.0f, float(db_current - db_start) / float(DBSize)));
      }
    }
    else
    {
      break;
    }
  }

  return new LinuxResolver(std::move(rdc_cache));
}

static inline void execute_addr2line(char addr2line_result[4096], char const *filename, uintptr_t addr2line_addr)
{
  addr2line_result[0] = '\0';
  int sv_stdout[2];
  int sv_stderr[2];
  int ret_sv_stdout = socketpair(AF_UNIX, SOCK_STREAM, 0, sv_stdout);
  int ret_sv_stderr = socketpair(AF_UNIX, SOCK_STREAM, 0, sv_stderr);
  if (ret_sv_stdout != -1 && ret_sv_stderr != -1)
  {
    shutdown(sv_stdout[0], SHUT_WR);
    shutdown(sv_stdout[1], SHUT_RD);
    shutdown(sv_stderr[0], SHUT_WR);
    shutdown(sv_stderr[1], SHUT_RD);

    pid_t pid_child = fork();
    if (pid_child == 0)
    {
      close(sv_stdout[0]);
      close(sv_stderr[0]);

      close(STDOUT_FILENO);
      dup2(sv_stdout[1], STDOUT_FILENO);
      close(sv_stdout[1]);
      close(STDERR_FILENO);
      dup2(sv_stderr[1], STDERR_FILENO);
      close(sv_stderr[1]);

      char arg_0[] = {"addr2line"};
      char arg_1[] = {"-fCe"};
      char arg_2[4096];
      char arg_3[] = {"ffffffffffffffff"};
      snprintf(arg_2, 4096, "%s", filename);
      snprintf(arg_3, sizeof(arg_3) / sizeof(char), "%" PRIxPTR, addr2line_addr);
      char *argv[] = {arg_0, arg_1, arg_2, arg_3, NULL};
      int ret_evec = execvp(argv[0], argv);
      char addr2line_stderr[4096];
      sprintf(addr2line_stderr, "Fail to execute command:\"%s %s %s %s\". Errno: %s.", argv[0], argv[1], argv[2], argv[3], strerror(errno));
      write(STDERR_FILENO, addr2line_stderr, strlen(addr2line_stderr));
      _exit(ret_evec); //There may be locks in std::exit
    }

    close(sv_stdout[1]);
    close(sv_stderr[1]);

    if (pid_child != -1)
    {
      int stat_loc;
      if (waitpid(pid_child, &stat_loc, 0) != -1) //release zombie
      {
        if ((WIFEXITED(stat_loc)))
        {
          if ((WEXITSTATUS(stat_loc)) == 0)
          {
            int ret_rd = read(sv_stdout[0], addr2line_result, 4095);
            if (ret_rd != -1)
            {
              addr2line_result[ret_rd] = '\0';
            }
            else
            {
              addr2line_result[0] = '\0';
            }
          }
          else
          {
            char addr2line_stdout[4096];
            int ret_rd_stdout = read(sv_stdout[0], addr2line_stdout, 4095);
            if (ret_rd_stdout != -1 && ret_rd_stdout != 0)
            {
              addr2line_stdout[ret_rd_stdout] = '\0';
              for (int i = 0; i < ret_rd_stdout; ++i)
              {
                if (addr2line_stdout[i] == '\r' || addr2line_stdout[i] == '\n' || addr2line_stdout[i] == '\0')
                {
                  addr2line_stdout[i] = ' ';
                }
              }
              RDCLOG("%s", addr2line_stdout);
            }

            char addr2line_stderr[4096];
            int ret_rd_stderr = read(sv_stderr[0], addr2line_stderr, 4095);
            if (ret_rd_stderr != -1 && ret_rd_stderr != 0)
            {
              addr2line_stderr[ret_rd_stderr] = '\0';
              for (int i = 0; i < ret_rd_stderr; ++i)
              {
                if (addr2line_stderr[i] == '\r' || addr2line_stderr[i] == '\n' || addr2line_stderr[i] == '\0')
                {
                  addr2line_stderr[i] = ' ';
                }
              }
              RDCWARN("%s", addr2line_stderr);
            }
          }
        }
      }
    }

    close(sv_stdout[0]);
    close(sv_stderr[0]);
  }
}