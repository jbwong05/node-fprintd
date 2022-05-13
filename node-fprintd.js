var node_fprintd = require("bindings")("node-fprintd");

function supportsBiometrics() {
  return node_fprintd.supportsBiometrics();
}

function authenticateBiometric() {
  return node_fprintd.authenticateBiometric();
}

export { supportsBiometrics, authenticateBiometric };
