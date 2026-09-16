import { readFileSync } from 'node:fs'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const here = dirname(fileURLToPath(import.meta.url))
const root = dirname(dirname(here))

const { default: createVeil } = await import(join(here, '..', 'dist', 'veil.mjs'))

const log = []
const Module = await createVeil({ print: (t) => log.push(t), printErr: (t) => log.push(t) })

if (Module.ccall('veil_init', 'number', [], []) !== 0)
  throw new Error('veil_init failed')

const call = (name, types, args) => {
  const pointer = Module.ccall(name, 'number', types, args)
  if (!pointer) return null
  const text = Module.UTF8ToString(pointer)
  Module.ccall('veil_free', null, ['number'], [pointer])
  return JSON.parse(text)
}

const encode = (carrier, secret, output, passphrase) =>
  call('veil_encode', ['string', 'string', 'string', 'string'], [carrier, secret, output, passphrase])

const decode = (target, output, passphrase) =>
  call('veil_decode', ['string', 'string', 'string'], [target, output, passphrase])

Module.FS.mkdir('/work')

let failures = 0
const check = (label, ok, detail = '') => {
  console.log(`${ok ? 'ok  ' : 'FAIL'}  ${label}${detail ? `  ${detail}` : ''}`)
  if (!ok) failures++
}

const secret = Buffer.from('the quick brown fox jumps over the lazy dog\n'.repeat(24))

for (const [carrier, passphrase] of [
  ['veil.png', ''],
  ['veil.png', 'correct horse battery staple'],
  ['veil.jpg', ''],
  ['veil.jpg', 'correct horse battery staple'],
]) {
  const label = `${carrier} / ${passphrase ? 'encrypted' : 'plain'}`
  log.length = 0

  Module.FS.writeFile('/work/carrier', readFileSync(join(root, 'assets', carrier)))
  Module.FS.writeFile('/work/secret', secret)

  const encoded = encode('/work/carrier', '/work/secret', '/work/stego', passphrase)
  check(`${label}: encode`, encoded?.ok === true, encoded?.ok ? '' : encoded?.error ?? log.join(' | '))
  if (!encoded?.ok) continue

  check(`${label}: encode reports encryption`, encoded.encrypted === Boolean(passphrase))

  const untouched = decode('/work/carrier', '/work/out', passphrase)
  check(`${label}: decode of the untouched carrier fails`, untouched?.ok === false)

  const decoded = decode('/work/stego', '/work/out', passphrase)
  check(`${label}: decode`, decoded?.ok === true && decoded.bytes === secret.length,
    decoded?.ok ? `${decoded.bytes} bytes` : decoded?.error ?? log.join(' | '))
  if (!decoded?.ok) continue

  check(`${label}: round trip`, Buffer.from(Module.FS.readFile('/work/out')).equals(secret))

  if (passphrase) {
    const wrong = decode('/work/stego', '/work/out', 'not the passphrase')
    check(`${label}: wrong passphrase is rejected`, wrong?.ok === false)
  }
}

log.length = 0
Module.FS.writeFile('/work/junk', Buffer.from('not an image at all'))
Module.FS.writeFile('/work/secret', secret)
const junk = encode('/work/junk', '/work/secret', '/work/stego', '')
check('a non-image is refused', junk?.ok === false)

console.log(failures ? `\n${failures} failed` : '\nall passed')
process.exit(failures ? 1 : 0)
