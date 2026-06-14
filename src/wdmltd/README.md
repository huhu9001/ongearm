A waydroid keyboard mapper for some niche usage. Needs

[https://github.com/oliviermugishak/phantom/blob/main/contrib/android-server/src/com/phantom/server/PhantomServer.java]

to work.

# Usage

`wdmltd <run|quit|pause|resume|song|config> [OPTION]`

# Actions
- run             Run the daemon. Need super user.
- quit            Destroy the daemon.
- pause, resume   Disable/Enable keyboard input.
- song            Load a CSV file.
- config          Reload or change the config file.

# Options
- --config        Specify the configuration file. Default to a `config.toml` in the the binary directory.
- --port          Port of the daemon. Default to the value in the config.
- --ip            IP of the Waydroid container. Default to the value in the config.
- --kbd           The file name of the keyboard to monitor. If not given, finds a proper one in `/dev/input/by-id`.
- --screen        CSV file-related argument.
                  `X_LEFT_UNIT,X_RIGHT_UNIT,Y_UNIT,X_LEFT_SOLO,X_RIGHT_SOLO,Y_UNIT[,X_LEFT_2M,X_RIGHT_2M,SLOP]`
                  
# Config
- port            Default value of `--port`. Default to 43212.
- song-path       Default path used by the action `song`. Default to the binary directory.
- ptsvr-path      The path of the d8-compiled jar of PhantomServer within Waydroid. Can only connect to already-running PhantomServer if not given.
- phantom.ip      Default value of `--ip`. Default to the value given by `waydroid status`.
- phantom.port    Port of the PhantomServer. Default to 27183.
- ctrl            Array. Controllers.
- ctrl[].type     `"tap"|"dpad"|"swipe"|"pause"|"song"`
- ctrl[].key      Key to trigger the controls other than `type="dpad"`.

- ctrl[].up
- ctrl[].down
- ctrl[].left
- ctrl[].right    Key to trigger `type="dpad"`.

- ctrl[].shift
- ctrl[].ctrl
- ctrl[].alt Key  Key modifiers.

- ctrl[].x
- ctrl[].y        Needed by `"tap"|"dpad"`

- ctrl[].r        Needed by `"dpad"`

- ctrl[].x1
- ctrl[].y1
- ctrl[].x2
- ctrl[].y1       Needed by `"swipe"`

- ctrl[].time     Used by `"swipe"`. Milliseconds. Default to 100.
