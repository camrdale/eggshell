#include <iostream>
#include <poll.h>
#include <unistd.h>

#include <lib/async-loop/cpp/loop.h>
#include <lib/syslog/cpp/log_settings.h>
#include <lib/syslog/cpp/macros.h>

#include "lib/async-loop/default.h"
#include "src/lib/fsl/tasks/fd_waiter.h"
#include "src/lib/line_input/modal_line_input.h"

namespace eggshell {

void OnAccept(const std::string& line, line_input::ModalLineInputStdout& line_input) {
  line_input.AddToHistory(line);
  std::cout << "I got: " << line << std::endl;
}

int ConsoleMain(int argc, const char** argv) {
  syslog::SetTags({"eggshell"});

  std::cout << "Welcome to the EggShell!" << std::endl;

  if (isatty(STDIN_FILENO)) {
    std::cout << "This is a TTY" << std::endl;
  }

  async::Loop loop(&kAsyncLoopConfigNeverAttachToThread);
  loop.GetState();

  fsl::FDWaiter input_waiter(loop.dispatcher());

  line_input::ModalLineInputStdout line_input;
  line_input.Init([&line_input](const std::string& line) { OnAccept(line, line_input); }, "% ");
  line_input.SetEofCallback([&loop, &line_input] {
    std::cout << "EOF" << std::endl;
    line_input.Hide();
    loop.Quit();
  });

  line_input.Show();

  input_waiter.Wait(
      [&loop, &line_input](zx_status_t status, uint32_t observed) {
        if (status != ZX_OK) {
          std::cout << "ERROR status: " << status << std::endl;
          loop.Quit();
          return;
        }
        for (;;) {
          char ch = 0;
          ssize_t rv = read(STDIN_FILENO, &ch, 1);
          if (rv < 0) {
            std::cout << "ERROR rv<0: " << rv << " errno: " << errno << std::endl;
            loop.Quit();
            return;
          }
          if (rv == 0) {
            std::cout << "ERROR rv==0: " << rv << std::endl;
            loop.Quit();
            return;
          }
          line_input.OnInput(ch);
          if (loop.GetState() == ASYNC_LOOP_QUIT) {
            return;
          }
        }
      },
      STDIN_FILENO, POLLIN);
  
  loop.Run();
  return 0;
}

}  // namespace eggshell

int main(int argc, const char** argv) { return eggshell::ConsoleMain(argc, argv); }
