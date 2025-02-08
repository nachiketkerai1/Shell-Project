Created a shell in my operating systems class:

Features
-------------
  Interactive & Batch Modes:
  
  Interactive: Commands entered via a prompt (wsh> ).
  Batch: Commands read from a file. No prompt displayed.
  Command Execution:
  
  Built-in and external commands are executed through process creation (fork, exec, wait).
  Path resolution for commands based on $PATH.
  Built-in Commands:
  
  exit: Exits the shell.
  cd [directory]: Changes the working directory.
  export VAR=value: Sets an environment variable.
  local VAR=value: Sets a local shell variable.
  vars: Displays all local shell variables.
  history: Displays up to 5 previously executed commands.
  ls: Built-in version of the ls command.
  Variable Substitution:
  
  Environment and shell variables can be used in commands (e.g., cd $MY_VAR).
  Supports defining, updating, and clearing variables.
  Redirection Support:
  
  Input (<), output (>), append (>>), and combined (&>).
