
#include <ur_client_library/primary/primary_client.h>
#include <memory>
#include "ur_client_library/comm/shell_consumer.h"
#include "ur_client_library/primary/primary_consumer.h"
#include "ur_client_library/primary/primary_package.h"

const std::string DEFAULT_ROBOT_IP = "192.168.56.101";

class VersionMessageConsumer : public urcl::primary_interface::PrimaryConsumer
{
public:
  virtual bool consume(urcl::primary_interface::VersionMessage& pkg) override
  {
    std::cout << "Received version message: " << pkg.toString() << std::endl;
    return true;
  }
};

int main(int argc, char* argv[])
{
  // Parse the ip arguments if given
  std::string robot_ip = DEFAULT_ROBOT_IP;
  if (argc > 1)
  {
    robot_ip = std::string(argv[1]);
  }

  // Parse how may seconds to run
  int second_to_run = -1;
  if (argc > 2)
  {
    second_to_run = std::stoi(argv[2]);
  }

  urcl::comm::INotifier notifier;
  auto primary_client = std::make_shared<urcl::primary_interface::PrimaryClient>(robot_ip, notifier);

  // auto consumer = std::make_shared<urcl::comm::ShellConsumer<urcl::primary_interface::PrimaryPackage>>();
  auto consumer = std::make_shared<VersionMessageConsumer>();

  primary_client->start();
  primary_client->addPrimaryConsumer(consumer);

  const auto start_time = std::chrono::system_clock::now();
  while (second_to_run < 0 || std::chrono::system_clock::now() - start_time < std::chrono::seconds(second_to_run))
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  std::cout << "Timeout reached" << std::endl;

  return 0;
}
