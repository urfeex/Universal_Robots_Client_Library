// -- BEGIN LICENSE BLOCK ----------------------------------------------
// Copyright 2023 Universal Robots A/S
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the {copyright_holder} nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// -- END LICENSE BLOCK ------------------------------------------------

#include <gtest/gtest.h>

#include <ur_client_library/ur/dashboard_client.h>
#define private public
#include <ur_client_library/ur/ur_driver.h>

using namespace urcl;

const std::string SCRIPT_FILE = "../resources/external_control.urscript";
const std::string OUTPUT_RECIPE = "resources/rtde_output_recipe.txt";
const std::string INPUT_RECIPE = "resources/rtde_input_recipe.txt";
const std::string CALIBRATION_CHECKSUM = "calib_12788084448423163542";
std::string g_ROBOT_IP = "192.168.56.101";

std::unique_ptr<UrDriver> g_ur_driver;
std::unique_ptr<DashboardClient> g_dashboard_client;

bool g_program_running;
std::condition_variable g_program_not_running_cv;
std::mutex g_program_not_running_mutex;
std::condition_variable g_program_running_cv;
std::mutex g_program_running_mutex;

// Helper functions for the driver
void handleRobotProgramState(bool program_running)
{
  // Print the text in green so we see it better
  std::cout << "\033[1;32mProgram running: " << std::boolalpha << program_running << "\033[0m\n" << std::endl;
  if (program_running)
  {
    std::lock_guard<std::mutex> lk(g_program_running_mutex);
    g_program_running = program_running;
    g_program_running_cv.notify_one();
  }
  else
  {
    std::lock_guard<std::mutex> lk(g_program_not_running_mutex);
    g_program_running = program_running;
    g_program_not_running_cv.notify_one();
  }
}

bool g_rtde_read_thread_running = false;
bool g_consume_rtde_packages = false;
std::mutex g_read_package_mutex;
std::thread g_rtde_read_thread;

void rtdeConsumeThread()
{
  while (g_rtde_read_thread_running)
  {
    // Consume package to prevent pipeline overflow
    if (g_consume_rtde_packages == true)
    {
      std::lock_guard<std::mutex> lk(g_read_package_mutex);
      std::unique_ptr<rtde_interface::DataPackage> data_pkg;
      data_pkg = g_ur_driver->getDataPackage();
    }
    else
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
}

class UrDriverTest : public ::testing::Test
{
protected:
  static void SetUpTestSuite()
  {
    g_dashboard_client.reset(new DashboardClient(g_ROBOT_IP));
    ASSERT_TRUE(g_dashboard_client->connect());

    // Make robot ready for test
    timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    g_dashboard_client->setReceiveTimeout(tv);

    // Stop running program if there is one
    ASSERT_TRUE(g_dashboard_client->commandStop());

    // if the robot is not powered on and ready
    ASSERT_TRUE(g_dashboard_client->commandBrakeRelease());

    // Setup driver
    std::unique_ptr<ToolCommSetup> tool_comm_setup;
    const bool headless = true;
    try
    {
      g_ur_driver.reset(new UrDriver(g_ROBOT_IP, SCRIPT_FILE, OUTPUT_RECIPE, INPUT_RECIPE, &handleRobotProgramState,
                                     headless, std::move(tool_comm_setup), CALIBRATION_CHECKSUM));
    }
    catch (UrException& exp)
    {
      std::cout << "caught exception " << exp.what() << " while launch driver, retrying once in 10 seconds"
                << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(10));
      g_ur_driver.reset(new UrDriver(g_ROBOT_IP, SCRIPT_FILE, OUTPUT_RECIPE, INPUT_RECIPE, &handleRobotProgramState,
                                     headless, std::move(tool_comm_setup), CALIBRATION_CHECKSUM));
    }
    g_ur_driver->startRTDECommunication();
    // Setup rtde read thread
    g_rtde_read_thread_running = true;
    g_rtde_read_thread = std::thread(rtdeConsumeThread);
  }

  void SetUp()
  {
    step_time_ = 0.002;
    if (g_ur_driver->getVersion().major < 5)
    {
      step_time_ = 0.008;
    }
    // Make sure script is running on the robot
    if (g_program_running == false)
    {
      g_consume_rtde_packages = true;
      g_ur_driver->sendRobotProgram();
      ASSERT_TRUE(waitForProgramRunning(1000));
    }
    g_consume_rtde_packages = false;
  }

  static void TearDownTestSuite()
  {
    g_rtde_read_thread_running = false;
    g_rtde_read_thread.join();
    g_dashboard_client->disconnect();
  }

  void readDataPackage(std::unique_ptr<rtde_interface::DataPackage>& data_pkg)
  {
    if (g_consume_rtde_packages == true)
    {
      URCL_LOG_ERROR("Unable to read packages while consuming, this should not happen!");
      GTEST_FAIL();
    }
    std::lock_guard<std::mutex> lk(g_read_package_mutex);
    data_pkg = g_ur_driver->getDataPackage();
    if (data_pkg == nullptr)
    {
      URCL_LOG_ERROR("Timed out waiting for a new package from the robot");
      GTEST_FAIL();
    }
  }

  bool waitForProgramRunning(int milliseconds = 100)
  {
    std::unique_lock<std::mutex> lk(g_program_running_mutex);
    if (g_program_running_cv.wait_for(lk, std::chrono::milliseconds(milliseconds)) == std::cv_status::no_timeout ||
        g_program_running == true)
    {
      return true;
    }
    return false;
  }

  bool waitForProgramNotRunning(int milliseconds = 100)
  {
    std::unique_lock<std::mutex> lk(g_program_not_running_mutex);
    if (g_program_not_running_cv.wait_for(lk, std::chrono::milliseconds(milliseconds)) == std::cv_status::no_timeout ||
        g_program_running == false)
    {
      return true;
    }
    return false;
  }

  // Robot step time
  double step_time_;
};

TEST_F(UrDriverTest, read_non_existing_script_file)
{
  g_consume_rtde_packages = true;
  const std::string non_existing_script_file = "";
  EXPECT_THROW(UrDriver::readScriptFile(non_existing_script_file), UrException);
}

TEST_F(UrDriverTest, read_existing_script_file)
{
  g_consume_rtde_packages = true;
  char existing_script_file[] = "urscript.XXXXXX";
  int fd = mkstemp(existing_script_file);
  if (fd == -1)
  {
    std::cout << "Failed to create temporary files" << std::endl;
    GTEST_FAIL();
  }
  EXPECT_NO_THROW(UrDriver::readScriptFile(existing_script_file));

  // clean up
  close(fd);
  unlink(existing_script_file);
}

TEST_F(UrDriverTest, robot_receive_timeout)
{
  g_consume_rtde_packages = true;

  // Robot program should time out after the robot receive timeout, whether it takes exactly 200 ms is not so important
  vector6d_t zeros = { 0, 0, 0, 0, 0, 0 };
  g_ur_driver->writeJointCommand(zeros, comm::ControlMode::MODE_IDLE, RobotReceiveTimeout::millisec(200));
  EXPECT_TRUE(waitForProgramNotRunning(400));

  // Start robot program
  g_ur_driver->sendRobotProgram();
  EXPECT_TRUE(waitForProgramRunning(1000));

  // Robot program should time out after the robot receive timeout, whether it takes exactly 200 ms is not so important
  g_ur_driver->writeFreedriveControlMessage(control::FreedriveControlMessage::FREEDRIVE_NOOP,
                                            RobotReceiveTimeout::millisec(200));
  EXPECT_TRUE(waitForProgramNotRunning(400));

  // Start robot program
  g_ur_driver->sendRobotProgram();
  EXPECT_TRUE(waitForProgramRunning(1000));

  // Robot program should time out after the robot receive timeout, whether it takes exactly 200 ms is not so important
  g_ur_driver->writeTrajectoryControlMessage(control::TrajectoryControlMessage::TRAJECTORY_NOOP, -1,
                                             RobotReceiveTimeout::millisec(200));
  EXPECT_TRUE(waitForProgramNotRunning(400));

  // Start robot program
  g_ur_driver->sendRobotProgram();
  EXPECT_TRUE(waitForProgramRunning(1000));

  // Robot program should time out after the robot receive timeout, whether it takes exactly 200 ms is not so important
  g_ur_driver->writeKeepalive(RobotReceiveTimeout::millisec(200));
  EXPECT_TRUE(waitForProgramNotRunning(400));
}

TEST_F(UrDriverTest, robot_receive_timeout_off)
{
  g_consume_rtde_packages = true;

  // Program should keep running when setting receive timeout off
  g_ur_driver->writeKeepalive(RobotReceiveTimeout::off());
  EXPECT_FALSE(waitForProgramNotRunning(1000));

  // Program should keep running when setting receive timeout off
  g_ur_driver->writeFreedriveControlMessage(control::FreedriveControlMessage::FREEDRIVE_NOOP,
                                            RobotReceiveTimeout::off());
  EXPECT_FALSE(waitForProgramNotRunning(1000));

  // Program should keep running when setting receive timeout off
  g_ur_driver->writeTrajectoryControlMessage(control::TrajectoryControlMessage::TRAJECTORY_NOOP, -1,
                                             RobotReceiveTimeout::off());
  EXPECT_FALSE(waitForProgramNotRunning(1000));

  // It shouldn't be possible to set robot receive timeout off, when dealing with realtime commands
  vector6d_t zeros = { 0, 0, 0, 0, 0, 0 };
  g_ur_driver->writeJointCommand(zeros, comm::ControlMode::MODE_SPEEDJ, RobotReceiveTimeout::off());
  EXPECT_TRUE(waitForProgramNotRunning(400));
}

TEST_F(UrDriverTest, stop_robot_control)
{
  g_consume_rtde_packages = true;

  vector6d_t zeros = { 0, 0, 0, 0, 0, 0 };
  g_ur_driver->writeJointCommand(zeros, comm::ControlMode::MODE_IDLE, RobotReceiveTimeout::off());

  // Make sure that we can stop the robot control, when robot receive timeout has been set off
  g_ur_driver->stopControl();
  EXPECT_TRUE(waitForProgramNotRunning(400));
}

TEST_F(UrDriverTest, target_outside_limits_servoj)
{
  std::unique_ptr<rtde_interface::DataPackage> data_pkg;
  readDataPackage(data_pkg);

  urcl::vector6d_t joint_positions_before;
  ASSERT_TRUE(data_pkg->getData("actual_q", joint_positions_before));

  // Create physically unfeasible target
  urcl::vector6d_t joint_target = joint_positions_before;
  joint_target[5] -= 2.5;

  // Send unfeasible targets to the robot
  readDataPackage(data_pkg);
  g_ur_driver->writeJointCommand(joint_target, comm::ControlMode::MODE_SERVOJ, RobotReceiveTimeout::millisec(200));

  // Ensure that the robot didn't move
  readDataPackage(data_pkg);
  urcl::vector6d_t joint_positions;
  ASSERT_TRUE(data_pkg->getData("actual_q", joint_positions));
  for (unsigned int i = 0; i < 6; ++i)
  {
    EXPECT_FLOAT_EQ(joint_positions_before[i], joint_positions[i]);
  }

  // Make sure the program is stopped
  g_consume_rtde_packages = true;
  g_ur_driver->stopControl();
  waitForProgramNotRunning(1000);
}

TEST_F(UrDriverTest, target_outside_limits_pose)
{
  std::unique_ptr<rtde_interface::DataPackage> data_pkg;
  readDataPackage(data_pkg);

  urcl::vector6d_t tcp_pose_before;
  ASSERT_TRUE(data_pkg->getData("actual_TCP_pose", tcp_pose_before));

  // Create physically unfeasible target
  urcl::vector6d_t tcp_target = tcp_pose_before;
  tcp_target[2] += 0.3;

  // Send unfeasible targets to the robot
  readDataPackage(data_pkg);
  g_ur_driver->writeJointCommand(tcp_target, comm::ControlMode::MODE_POSE, RobotReceiveTimeout::millisec(200));

  // Ensure that the robot didn't move
  readDataPackage(data_pkg);
  urcl::vector6d_t tcp_pose;
  ASSERT_TRUE(data_pkg->getData("actual_TCP_pose", tcp_pose));
  for (unsigned int i = 0; i < 6; ++i)
  {
    EXPECT_FLOAT_EQ(tcp_pose_before[i], tcp_pose[i]);
  }

  // Make sure the program is stopped
  g_consume_rtde_packages = true;
  g_ur_driver->stopControl();
  waitForProgramNotRunning(1000);
}

TEST_F(UrDriverTest, send_robot_program_retry_on_failure)
{
  // Start robot program
  g_ur_driver->sendRobotProgram();
  EXPECT_TRUE(waitForProgramRunning(1000));

  // Check that sendRobotProgram is robust to the secondary stream being disconnected. This is what happens when
  // switching from Remote to Local and back to Remote mode for example.
  g_ur_driver->secondary_stream_->close();

  EXPECT_TRUE(g_ur_driver->sendRobotProgram());
}

TEST_F(UrDriverTest, reset_rtde_client)
{
  double target_frequency = 50;
  g_ur_driver->resetRTDEClient(OUTPUT_RECIPE, INPUT_RECIPE, target_frequency);
  ASSERT_EQ(g_ur_driver->getControlFrequency(), target_frequency);
}

TEST_F(UrDriverTest, read_error_code)
{
  g_consume_rtde_packages = true;
  g_ur_driver->startPrimaryClientCommunication();
  // Wait until we actually received a package
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  std::stringstream cmd;
  cmd << "sec setup():" << std::endl << " protective_stop()" << std::endl << "end";
  EXPECT_TRUE(g_ur_driver->sendScript(cmd.str()));

  auto error_codes = g_ur_driver->getErrorCodes();
  while (error_codes.size() == 0)
  {
    error_codes = g_ur_driver->getErrorCodes();
  }

  ASSERT_EQ(error_codes.size(), 1);
  // Error code description can be found here:
  // https://myur.universal-robots.com/manuals/content/SW_5_16/Documentation%20Menu/Error%20Codes/Introduction/C209%20A%20protective%20stop%20was%20triggered%20%28for%20test%20purposes%20only%29
  ASSERT_EQ(error_codes.at(0).message_code, 209);
  ASSERT_EQ(error_codes.at(0).message_argument, 0);

  // Wait for after PSTOP before clearing it
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_TRUE(g_dashboard_client->commandCloseSafetyPopup());
  EXPECT_TRUE(g_dashboard_client->commandUnlockProtectiveStop());
}

// TODO we should add more tests for the UrDriver class.

int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);

  for (int i = 0; i < argc; i++)
  {
    if (std::string(argv[i]) == "--robot_ip" && i + 1 < argc)
    {
      g_ROBOT_IP = argv[i + 1];
      break;
    }
  }

  return RUN_ALL_TESTS();
}