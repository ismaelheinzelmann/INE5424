## INE5424
Socket reliable message library for INE5424 course at UFSC.

### Project Structure

#### The project is organized in four main folders
- **client:** The example client for the library test
- **config:** The configuration files for the client
- **doc:** The project documentation
- **lib:** The library itself

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

You can also define a Log Level, for example:

```
./client <node_id> DEBUG
```

The full list of Log levels is:
- INFO
- DEBUG
- WARNING
- ERROR
- FATAL

