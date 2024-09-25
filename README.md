## INE5424
Socket reliable message library for INE5424 course at UFSC.

### Project Structure

#### The project is organized in four main folders
- client: The example client for the library test
- config: The configuration files for the client
- doc: The project documentation
- lib: The library itself

#### The internal client and library structure is defined as:

- build: A special directory that should not be considered part of the source of the project. Used for storing ephemeral build results. must not be checked into source control. If using source control, must be ignored using source control ignore-lists.

- src: Main compilable source location. Must be present for projects with compiled components that do not use submodules. In the presence of include/, also contains private headers.

- include: Directory for public headers. May be present. May be omitted for projects that do not distinguish between private/public headers. May be omitted for projects that use submodules.

- tests: Directory for tests.

- extras: Directory containing extra/optional submodules for the project.

- data: Directory containing non-source code aspects of the project. This might include graphics and markup files.

- tools: Directory containing development utilities, such as build and refactoring scripts

#### Library Structure

#### Compiling

For compiling the library and compile the client, you can use the following commands:

```bash
make
```

For cleaning the project, you can use the following command:

```bash
make .clean
```

For building in debug mode, you can use the following command:

```bash
make .debug
```

#### Running

For running the client, you can use the following command:

```
./client <node_id> 
```

#### Logging

You can also use more than one Log level, for example:

```
./client <node_id> DEBUG
```

The full list of Log levels is:
- INFO
- DEBUG
- WARNING
- ERROR
- FATAL

