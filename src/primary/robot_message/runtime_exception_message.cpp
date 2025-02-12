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

#include "ur_client_library/primary/robot_message/runtime_exception_message.h"
#include "ur_client_library/primary/abstract_primary_consumer.h"

namespace urcl
{
namespace primary_interface
{
RuntimeExceptionMessage::RuntimeExceptionMessage(const RuntimeExceptionMessage& pkg)
  : RobotMessage(pkg.timestamp_, pkg.source_, RobotMessagePackageType::ROBOT_MESSAGE_RUNTIME_EXCEPTION)
{
  line_number_ = pkg.line_number_;
  column_number_ = pkg.column_number_;
  text_ = pkg.text_;
}

bool RuntimeExceptionMessage::parseWith(comm::BinParser& bp)
{
  bp.parse(line_number_);
  bp.parse(column_number_);
  bp.parseRemainder(text_);

  return true;  // not really possible to check dynamic size packets
}

bool RuntimeExceptionMessage::consumeWith(AbstractPrimaryConsumer& consumer)
{
  return consumer.consume(*this);
}

std::string RuntimeExceptionMessage::toString() const
{
  std::stringstream ss;
  ss << "Runtime error in line " << line_number_;
  ss << ", column " << column_number_ << std::endl;
  ss << "Error: " << text_;
  return ss.str();
}
}  // namespace primary_interface
}  // namespace urcl
