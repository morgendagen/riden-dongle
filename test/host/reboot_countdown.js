// Run the actual embedded scripts with a stalled navigation and a minimal DOM.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const text = fs.readFileSync('src/riden_http_server/http_static.h', 'utf8');
for (const name of ['HTML_REBOOTING_DONGLE_BODY', 'HTML_REBOOTING_DONGLE_UPDATE_BODY']) {
  const start = text.indexOf(`static const char ${name}[]`);
  const end = text.indexOf('\n\n', start);
  const strings = text.slice(start, end).match(/"(?:\\.|[^"\\])*"/g);
  const html = strings.map(s => JSON.parse(s)).join('');
  const script = html.match(/<script>(.*?)<\/script>/s)[1];
  const counter = {textContent: html.match(/id='counter'>(\d+)</)[1]};
  let callback, active = true, clears = 0, redirects = 0;
  const context = {
    Math,
    document: {querySelector: selector => {assert.equal(selector, '#counter');return counter;}},
    setInterval: (fn, ms) => {assert.equal(ms, 1000);callback = fn;return 17;},
    clearInterval: id => {assert.equal(id, 17);active = false;++clears;},
    window: {location: {replace: url => {assert.equal(url, '/');++redirects;}}},
  };
  vm.runInNewContext(script, context);
  const seconds = Number(counter.textContent);
  for (let tick = 0; tick < seconds + 20; ++tick) {
    if (active) callback();
    assert(Number(counter.textContent) >= 0);
  }
  assert.equal(Number(counter.textContent), 0);
  assert.equal(clears, 1);
  assert.equal(redirects, 1);
  console.log(`PASS: ${name} stops at zero and redirects once even if navigation stalls`);
}
