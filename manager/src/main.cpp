/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <string>
#include <fstream>

#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "Comm.h"
#include "Context.h"
#include "QueueManager.h"
#include "BlockManager.h"
#include "TaskManager.h"

using namespace blaze;

int main(int argc, char** argv) {
  
  std::string conf_path = "./conf.prototxt";

  if (argc < 2) {
    printf("USAGE: %s <conf_path>\n", argv[0]);
    return -1;
  }

  int file_handle = open(argv[1], O_RDONLY);

  if (file_handle < 0) {
    printf("cannot find configure file: %s\n",
        conf_path.c_str());
    return -1;
  }
  
  ManagerConf *conf = new ManagerConf();
  google::protobuf::io::FileInputStream fin(file_handle);
  
  if (!google::protobuf::TextFormat::Parse(&fin, conf)) {
    printf("cannot parse configuration from %s\n", argv[1]);  
    return 1;
  }

  // setup Logger
  int verbose = conf->verbose();  
  Logger logger(verbose);

  // setup QueueManager
  QueueManager queue_manager(&logger);

  // setup acc context
  Context context(conf, &logger, &queue_manager);

  queue_manager.startAll();

  // check all network interfaces on this computer, and 
  // open a communicator on each interface using the same port
  int port = conf->port();

  struct ifaddrs* ifAddrStruct = NULL;
  getifaddrs(&ifAddrStruct);

  // hold all pointers to the communicator
  std::vector<boost::shared_ptr<Comm> > comm_pool;

  for (struct ifaddrs* ifa = ifAddrStruct; 
       ifa != NULL; 
       ifa = ifa->ifa_next) 
  {
    if (!ifa->ifa_addr) {
      continue;
    }
    // check if is a valid IP4 Address
    if (ifa->ifa_addr->sa_family == AF_INET) {

      // obtain the ip address as a string
      void* tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
      char addressBuffer[INET_ADDRSTRLEN];

      inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

      std::string ip_addr(addressBuffer);

      // create communicator object
      // it will start listening for new connections automatically
      boost::shared_ptr<Comm> comm( new Comm(
            &context, 
            &queue_manager, 
            &logger, ip_addr, port));

      // push the communicator pointer to pool to avoid object
      // being destroyed out of context
      comm_pool.push_back(comm);
    }
  }

  // if no endpoint in the config, skip this part
  while (1) {
    // potential place for cleaning stage
    ;
  }

  return 0;
}
