#include "httpd.h"
#include "confd_shmtx.h"
#include "config.h"
#include "confd_dict.h"
#include "confd_shm.h"
#include <boost/foreach.hpp>
#include "../lib/jsoncpp/src/json/json.h"


extern confd_shm_t   *shm;
extern confd_shmtx_t *shmtx;

using namespace std;
// Added for the json-example:
using namespace boost::property_tree;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;



int httpServer(std::string address, int port) {
  // HTTP-server at port 8080 using 1 thread
  // Unless you do more heavy non-threaded processing in the resources,
  // 1 thread is usually faster than several threads
  HttpServer server;
  server.config.port = port;
  server.config.address = address;
  server.config.reuse_port = true;

  // Add resources using path-regex and method-string, and an anonymous function
  // POST-example for the path /string, responds the posted string
  // POST-example for the path /json, responds firstName+" "+lastName from the posted json
  // Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
  // Example posted json:
  // {
  //   "firstName": "John",
  //   "lastName": "Smith",
  //   "age": 25
  // }
  server.resource["^/json$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    try {
      ptree pt;
      read_json(request->content, pt);

      auto name = pt.get<string>("firstName") + " " + pt.get<string>("lastName");

      *response << "HTTP/1.1 200 OK\r\n"
                << "Content-Length: " << name.length() << "\r\n\r\n"
                << name;
    }
    catch(const exception &e) {
      *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n"
                << e.what();
    }


  };

  server.resource["^/api/confs$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    string content;
    Json::Value root;
    try {
      ptree pt;
      read_json(request->content, pt);

      string domain = pt.get<string>("domain");
      string tmp_type = pt.get<string>("type");
      string listen_port = pt.get<string>("listen");
      ptree upstreams = pt.get_child("upstreams");  // get_child得到数组对象   
      
      std::vector<std::string> upstream_vcs;
      BOOST_FOREACH(boost::property_tree::ptree::value_type &v, upstreams) {  
          upstream_vcs.push_back(v.second.get<string>("server"));
      } 

      confd_dict dict;
      dict.shm_sync_to_dict();
      std::pair<bool, std::string> ret = dict.add_key(listen_port, domain, upstream_vcs, tmp_type);
      
      root["code"] = 200;
      root["info"] = ret.second;
    }
    catch(const exception &e) {
      root["code"] = 403;
      root["info"] = e.what();
    }
    content = root.toStyledString();
    *response << "HTTP/1.1 200 OK\r\n"
              << "Content-Length: " << content.length() << "\r\n\r\n"
              << content;
  };

  server.resource["^/api/confs$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    lock(shmtx);
    string content(shm->addr);
    unlock(shmtx);

    *response << "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;
  };

  server.resource["^/api/confs/((.*?):(.*?))"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    Json::Value root, data, upstreams;

    std::string search = request->path_match[1];
    confd_dict* dict = new confd_dict();
    dict->shm_sync_to_dict();
    std::pair<bool, std::vector<std::string>> ret = dict->get_value_by_key(search);
    if (ret.first) {
        root["code"] = 200;
        for(auto& item: ret.second) {
           upstreams.append(item); 
        }
        data["listen"] = string(request->path_match[3]);
        data["server_name"] = string(request->path_match[2]);
        data["upstream"] = upstreams;
    } else {
        root["code"] = 403;
    }
    root["data"] = data;
    delete dict;
    
    string content = root.toStyledString();
    *response << "HTTP/1.1 200 OK\r\n"
              << "Content-Type: application/json\r\n"
              << "Content-Length: " << content.length() << "\r\n\r\n"
              << content;
  };


  // GET-example for the path /match/[number], responds with the matched string in path (number)
  // For instance a request GET /match/123 will receive: 123
  // GET-example simulating heavy work in a separate thread
  server.resource["^/work$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    string content = "hello,world\n";
    *response << "HTTP/1.1 200 OK\r\n"
              << "Content-Length: " << content.length() << "\r\n\r\n"
              << content;
  };

  // Default GET-example. If no other matches, this anonymous function will be called.
  // Will respond with content in the web/-directory, and its subdirectories.
  // Default file: index.html
  // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
  server.default_resource["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    try {
      auto web_root_path = boost::filesystem::canonical("web");
      auto path = boost::filesystem::canonical(web_root_path / request->path);
      // Check if path is within web_root_path
      if(distance(web_root_path.begin(), web_root_path.end()) > distance(path.begin(), path.end()) ||
         !equal(web_root_path.begin(), web_root_path.end(), path.begin()))
        throw invalid_argument("path must be within root path");
      if(boost::filesystem::is_directory(path))
        path /= "index.html";

      SimpleWeb::CaseInsensitiveMultimap header;

      auto ifs = make_shared<ifstream>();
      ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

      if(*ifs) {
        auto length = ifs->tellg();
        ifs->seekg(0, ios::beg);

        header.emplace("Content-Length", to_string(length));
        response->write(header);

        // Trick to define a recursive function within this scope (for example purposes)
        class FileServer {
        public:
          static void read_and_send(const shared_ptr<HttpServer::Response> &response, const shared_ptr<ifstream> &ifs) {
            // Read and send 128 KB at a time
            static vector<char> buffer(131072); // Safe when server is running on one thread
            streamsize read_length;
            if((read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount()) > 0) {
              response->write(&buffer[0], read_length);
              if(read_length == static_cast<streamsize>(buffer.size())) {
                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                  if(!ec)
                    read_and_send(response, ifs);
                  else
                    cerr << "Connection interrupted" << endl;
                });
              }
            }
          }
        };
        FileServer::read_and_send(response, ifs);
      }
      else
        throw invalid_argument("could not read file");
    }
    catch(const exception &e) {
      response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what() + "\n");
    }
  };

  server.on_error = [](shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
    // Handle errors here
    // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
  };

  thread server_thread([&server]() {
    // Start server
    server.start();
  });

  server_thread.join();
  return 0;
}
