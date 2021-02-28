#include <iostream>
#include <poll.h>
#include <unistd.h>

#include <lib/async-loop/cpp/loop.h>
#include <lib/syslog/cpp/log_settings.h>
#include <lib/syslog/cpp/macros.h>

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/synchronous_interface_ptr.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/file_descriptor.h>
#include <lib/sys/cpp/termination_reason.h>
#include <lib/syslog/cpp/macros.h>
#include "lib/async-loop/default.h"
#include "src/lib/fsl/tasks/fd_waiter.h"
#include "src/lib/line_input/modal_line_input.h"
#include "src/lib/fxl/command_line.h"
#include "src/lib/fxl/log_settings_command_line.h"
#include "src/lib/fxl/macros.h"

namespace eggshell {

int ConsoleMain(int argc, const char** argv) {
  const auto command_line = fxl::CommandLineFromArgcArgv(argc, argv);
  fxl::SetLogSettingsFromCommandLine(command_line, {"eggshell"});

  std::cout << "Welcome to the EggShell!" << std::endl;

  if (isatty(STDIN_FILENO)) {
    std::cout << "This is a TTY" << std::endl;
  }

  async::Loop loop(&kAsyncLoopConfigNeverAttachToThread);
  std::unique_ptr<sys::ComponentContext> context =
    sys::ComponentContext::CreateAndServeOutgoingDirectory();
  
  fsl::FDWaiter input_waiter(loop.dispatcher());

  line_input::ModalLineInputStdout line_input;
  line_input.Init([&loop, &context, &line_input](const std::string& line) {
    line_input.AddToHistory(line);
    std::cout << "I got: " << line << std::endl;

    std::string buf;
    std::stringstream ss(line);
    std::vector<std::string> tokens;
    while (ss >> buf) tokens.push_back(buf);

    if (!tokens.empty() && tokens[0] == "run") {
      if (tokens.size() < 2) {
        std::cerr << "Usage: run <program> <args>*" << std::endl;
        return;
      }
      std::cout << "Running: " << tokens[1] << std::endl;

      fuchsia::sys::ComponentControllerPtr controller;
      controller.Unbind();
      fidl::InterfaceHandle<fuchsia::io::Directory> directory;

      fuchsia::sys::LaunchInfo launch_info;
      launch_info.url = tokens[1];
      launch_info.directory_request = directory.NewRequest().TakeChannel();
      launch_info.out = sys::CloneFileDescriptor(STDOUT_FILENO);
      launch_info.err = sys::CloneFileDescriptor(STDERR_FILENO);

      fuchsia::sys::LauncherSyncPtr launcher;
      context->svc()->Connect<fuchsia::sys::Launcher>(launcher.NewRequest());
      launcher->CreateComponent(std::move(launch_info), controller.NewRequest(loop.dispatcher()));
      controller.set_error_handler([&tokens](zx_status_t status) {
        std::cerr << "Connection error from EggShell to running: " << tokens[1];
      });
      controller.events().OnTerminated = [&tokens](int64_t return_code,
                                                fuchsia::sys::TerminationReason termination_reason) {
        std::cout << "Terminated: " << tokens[1] << " with reason: "
          << sys::HumanReadableTerminationReason(termination_reason);
      };
      std::cout << "Started: " << tokens[1] << std::endl;
    }
  }, "% ");
  line_input.SetEofCallback([&loop, &line_input] {
    std::cout << "EOF" << std::endl;
    line_input.Hide();
    loop.Quit();
  });

  line_input.Show();

  input_waiter.Wait(
      [&loop, &line_input](zx_status_t status, uint32_t observed) {
        if (status != ZX_OK) {
          std::cerr << "ERROR status: " << status << std::endl;
          loop.Quit();
          return;
        }
        for (;;) {
          char ch = 0;
          ssize_t rv = read(STDIN_FILENO, &ch, 1);
          if (rv < 0) {
            std::cerr << "ERROR rv<0: " << rv << " errno: " << errno << std::endl;
            loop.Quit();
            return;
          }
          if (rv == 0) {
            std::cerr << "ERROR rv==0: " << rv << std::endl;
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
