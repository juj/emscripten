function extractBoolCmdLineInput(args, param, defaultValue) {
    var idx = args.indexOf(param);
    if (idx != -1) {
      args.splice(idx, 1);
      return true;
    }
    return defaultValue;
  }
  
  function extractStringCmdLineInput(args, param, defaultValue) {
    var idx = args.indexOf(param);
    if (idx != -1) {
      var value = args[idx+1];
      args.splice(idx, 2);
      return value;
    }
    return defaultValue;
  }
  
  function extractNumberCmdLineInput(args, param, defaultValue) {
    var idx = args.indexOf(param);
    if (idx != -1) {
      var value = args[idx+1];
      args.splice(idx, 2);
      return parseFloat(value);
    }
    return defaultValue;
  }

  module.exports = {
    extractBoolCmdLineInput: extractBoolCmdLineInput,
    extractStringCmdLineInput: extractStringCmdLineInput,
    extractNumberCmdLineInput: extractNumberCmdLineInput
  };
  