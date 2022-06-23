Application Development         {#app-dev}
==============

## Overview

An application that uses VXG Cloud Agent Library should implement 3 classes derived from the base classes provided by the library:
- @ref vxg::cloud::agent::callback "agent::callback" - common callbacks class, only @ref vxg::cloud::agent::callback::on_bye "on_bye" callback is mandatory for implementation
- [agent::media::stream](@ref vxg::cloud::agent::media::stream) class, abstract class for media streams, library provides basic @ref vxg::cloud::agent::media::rtsp_stream "media::rtsp_stream" implementation which retransmits RTSP source stream to the endpoint of the VXG Cloud, all callbacks are stubbed. Developer normally should implement own class derived from the @ref vxg::cloud::agent::media::stream "media::stream" with own @ref vxg::media::Streamer::ISource "vxg::media::Streamer::ISource" implementation(vxg::media::ffmpeg::Source class implementation from the @ref ffmpeg_source.cc "ffmpeg_source.cc" can be used as a reference), or if RTSP source is acceptable developer can implement own class derived from the @ref vxg::cloud::agent::media::rtsp_stream "media::rtsp_stream" but with callbacks implemented.
- @ref vxg::cloud::agent::event_stream "agent::event_stream" class, abstract class for events generation.

Any callback implementation as well as @ref vxg::media::Streamer::ISource::init "ISource::init" and @ref vxg::media::Streamer::ISource::init "ISource::finit" implementations should be non-blocking, VXG Cloud messages processing is single-threaded which means any VXG Cloud messages are handled sequentially hence no new message will be processed until the callback triggered by the previous message is returned.

The library provides the stub implementation for most of the virtual methods of these classes, the stub implementation prints a log message about this method is not implemented and returns an error, the final application should implement all virtual methods on its own.

Most of the callbacks are just getter/setter for the library's [objects](@ref vxg::cloud::agent::proto).


## Examples
### Minimal application example

Headers and namespaces:
\snippet app/src/cloud-agent-minimal.cc Includes and namespaces

Common callbacks class, minimal implementation derived from the @ref vxg::cloud::agent::callback "agent::callback" class:
\snippet app/src/cloud-agent-minimal.cc Minimal callback class implementation

Create and start agent object @ref vxg::cloud::agent::manager "agent::manager" with one basic media stream @ref vxg::cloud::agent::media::rtsp_stream "agent::media::rtsp_stream"
\snippet app/src/cloud-agent-minimal.cc Create and start agent object

Complete minimal example:
\include app/src/cloud-agent-minimal.cc

### Complete application example

**Common callback class:** derived from @ref vxg::cloud::agent::callback "agent::callback"
\snippet app/src/cloud-agent.cc Common callback class implementation

**Media stream callback class:** derived from @ref vxg::cloud::agent::media::stream "agent::media::stream"
\snippet app/src/cloud-agent.cc Media stream callback class implementation

**Event stream callback class:** derived from @ref vxg::cloud::agent::event_stream "agent::media::event_stream"
\snippet app/src/cloud-agent.cc Event stream callback class implementation

**Creating and start agent instance with all callbacks:**
\snippet app/src/cloud-agent.cc Create and start agent object

### Linking application against the VXG Agent Cloud Library
There are 3 possible ways of how to build and link your application
1. Building the application inside the VXG CLoud Agent library's `Meson project`, the app will be assembled during the library project compilation in this case.  
You need to add a new executable target into the main `meson.build` file, please refer to the example app build target declaration:
\snippet app/meson.build meson-example-target
User must declare own executable target with a list of sources and dependencies, user may need to declare own dependencies if application requires it.  
  
  **This method is not recommended as it makes updating of the VXG Cloud Agent library mostly not possible or very difficult for application developer**
2. Building your app using your own build system and linking against the installed library.  
  Running the `install` step from the [compile](@ref build) section installs the binary libraries and headers into the directory you specified during the `setup` step, it also puts the `pkg-config`'s `.pc` files into the prefix directory which could be used by your own build system.
  
3. Preferred and recommended way of application development is to hold the app as a separate `Meson project` and use the VXG Cloud Agent library as a `Meson subproject` of the application's `Meson project`.  
Using this approach gives the most flexible and convenient workflow for updating the VXG Cloud Library, all library dependencies will be promoted to the main project and will be also accessible by the application.  
 **How does it work**
 - Assuming you have a `Meson` build system [installed](@ref build)
 - Start a new `Meson project` with a following command:
 ```
 meson init -l cpp -n your-project-name
 ```
 - As a result of this command you should have the following files tree:
 ```
 |-- meson.build
 |-- your_project_name.cpp
 ```
 - Add VXG Cloud Agent library as a `Meson subproject`  
 All subprojects should be located in the `subprojects` directory so you have to create it first
 ```
 mkdir subprojects
 ```
 Now you have 2 options depending on how you want to store the VXG Cloud Agent library sources:
  1. If you want to store the VXG Cloud Agent library as a files tree locally.
   + Create a symlink to the library path inside the subprojects dir:
   ```
   ln -s path/to/vxgcloudagent subprojects/vxgcloudagent
   ```
   Or you can just move vxgcloudagent directory inside the subprojects dir.
   + Create a library's `Meson` wrap file inside the subprojects dir, the name of the file should be the same as symlink you created in 1.1 and the content of the file should be:
   ```
   [wrap-file]
   directory = vxgcloudagent

   [provide]
   vxgcloudagent = vxgcloudagent_dep
   ```
  2. If you want to store the library in a git repository you just need to create a wrap file with the content like below:
   ```
   [wrap-git]
   url=https://your-git-repo-url.com/path/vxgcloudagent.git
   # You can specify tag, branch or commit hash as revision
   revision=master

   [provide]
   vxgcloudagent = vxgcloudagent_dep
   ```
 You can find the example app `Meson project` in the example/app directory of the VXG Cloud library sources package.







