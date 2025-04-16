// -- BEGIN LICENSE BLOCK ----------------------------------------------
// Copyright 2025 Universal Robots A/S
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

/*
 * @file long_term_test.cpp
 * @author Felix Exner
 * @date 2025-01-01
 * @brief This "test" is supposed to be running for a long period to verify stability of the URCL
 *
 * This test is designed to run for an extended period of time to check for memory leaks and other issues. It is not a
 * classic GTest and will not be running in the library's test suite.
 */

#include "ur_client_library/example_robot_wrapper.h"
#include "ur_client_library/ur/instruction_executor.h"

using namespace urcl;
const std::string DEFAULT_ROBOT_IP = "192.168.56.101";
const std::string SCRIPT_FILE = "resources/external_control.urscript";
const std::string OUTPUT_RECIPE = "tests/resources/rtde_output_recipe.txt";
const std::string INPUT_RECIPE = "tests/resources/rtde_input_recipe.txt";

class LongTermTest
{
public:
  LongTermTest(const std::string& ip_address) : ip_address_(ip_address)
  {
    robot_ = std::make_shared<ExampleRobotWrapper>(ip_address_, OUTPUT_RECIPE, INPUT_RECIPE);
    cycle_time_ = std::chrono::milliseconds(1000 / robot_->getUrDriver()->getControlFrequency());
    instruction_executor_ = std::make_shared<InstructionExecutor>(robot_->getUrDriver());
  }

  void run()
  {
    if (!robot_->isHealthy())
    {
      URCL_LOG_ERROR("Robot initialization failed.");
      throw std::runtime_error("Robot initialization failed.");
    }
    while (true)
    {
      runInstructionExecutorMotion();
    }
  }

private:
  std::shared_ptr<ExampleRobotWrapper> robot_;
  std::shared_ptr<InstructionExecutor> instruction_executor_;
  std::string ip_address_;
  std::chrono::milliseconds cycle_time_;

  void runInstructionExecutorMotion()
  {
    // Trajectory definition
    std::vector<std::shared_ptr<urcl::control::MotionPrimitive>> motion_sequence{
      std::make_shared<urcl::control::MoveJPrimitive>(urcl::vector6d_t{ -1.27, -1.57, 0, 0, 0, 0 }, 0.1,
                                                      std::chrono::seconds(5)),
      // This point uses acceleration / velocity parametrization
      std::make_shared<urcl::control::MoveJPrimitive>(urcl::vector6d_t{ -1.57, -1.6, 1.6, -0.7, 0.7, 0.2 }, 0.1,
                                                      std::chrono::seconds(0), 1.4, 2.0),

      std::make_shared<urcl::control::MoveLPrimitive>(urcl::Pose(-0.203, 0.263, 0.559, 0.68, -1.083, -2.076), 0.1,
                                                      std::chrono::seconds(2)),
      std::make_shared<urcl::control::MovePPrimitive>(urcl::Pose{ -0.203, 0.463, 0.559, 0.68, -1.083, -2.076 }, 0.1,
                                                      0.2, 0.2),
    };
    if (!instruction_executor_->executeMotion(motion_sequence))
    {
      URCL_LOG_ERROR("Motion execution failed.");
      throw std::runtime_error("Motion execution failed.");
    }
  }
};

int main(int argc, char* argv[])
{
  urcl::setLogLevel(urcl::LogLevel::INFO);

  // Parse the ip arguments if given
  std::string robot_ip = DEFAULT_ROBOT_IP;
  if (argc > 1)
  {
    robot_ip = std::string(argv[1]);
  }

  auto my_test = LongTermTest(robot_ip);

  try
  {
    my_test.run();
  }
  catch (const std::exception& e)
  {
    URCL_LOG_ERROR("Exception caught during execution: %s", e.what());
    return 1;
  }
}
