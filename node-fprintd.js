var node_fprintd = require("./build/Release/node-fprintd.node");

function supportsBiometrics() {
  return node_fprintd.supportsBiometrics();
}

function authenticateBiometric() {
  return node_fprintd.authenticateBiometric();
}

export { supportsBiometrics, authenticateBiometric };
