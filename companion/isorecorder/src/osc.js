// Minimal OSC 1.0 codec for ISO Recorder: messages with i / f / s arguments (what the program sends and understands).

function pad4(n) {
	return (n + 3) & ~3
}

function writeString(s) {
	const raw = Buffer.from(s, 'utf8')
	const out = Buffer.alloc(pad4(raw.length + 1))
	raw.copy(out)
	return out
}

/** @param {string} path @param {(number|string)[]} args */
export function encode(path, args = []) {
	let tags = ','
	const parts = [writeString(path)]
	const data = []
	for (const a of args) {
		if (typeof a === 'number' && Number.isInteger(a)) {
			tags += 'i'
			const b = Buffer.alloc(4)
			b.writeInt32BE(a)
			data.push(b)
		} else if (typeof a === 'number') {
			tags += 'f'
			const b = Buffer.alloc(4)
			b.writeFloatBE(a)
			data.push(b)
		} else {
			tags += 's'
			data.push(writeString(String(a)))
		}
	}
	parts.push(writeString(tags), ...data)
	return Buffer.concat(parts)
}

function readString(buf, pos) {
	const end = buf.indexOf(0, pos)
	if (end < 0) throw new Error('bad OSC string')
	return [buf.toString('utf8', pos, end), pad4(end + 1)]
}

/** Decodes one message (or the messages of a #bundle). Returns [{path, args}], [] for damaged data. */
export function decode(buf) {
	try {
		if (buf.toString('latin1', 0, 7) === '#bundle') {
			const out = []
			let pos = 16
			while (pos + 4 <= buf.length) {
				const len = buf.readInt32BE(pos)
				pos += 4
				if (len <= 0 || pos + len > buf.length) return []
				out.push(...decode(buf.subarray(pos, pos + len)))
				pos += len
			}
			return out
		}
		let [path, pos] = readString(buf, 0)
		if (!path.startsWith('/')) return []
		const args = []
		if (pos < buf.length) {
			let tags
			;[tags, pos] = readString(buf, pos)
			for (const t of tags.slice(1)) {
				if (t === 'i') {
					args.push(buf.readInt32BE(pos))
					pos += 4
				} else if (t === 'f') {
					args.push(buf.readFloatBE(pos))
					pos += 4
				} else if (t === 'd') {
					args.push(buf.readDoubleBE(pos))
					pos += 8
				} else if (t === 'h') {
					args.push(Number(buf.readBigInt64BE(pos)))
					pos += 8
				} else if (t === 's') {
					let s
					;[s, pos] = readString(buf, pos)
					args.push(s)
				} else if (t === 'T') args.push(1)
				else if (t === 'F') args.push(0)
				else return []
			}
		}
		return [{ path, args }]
	} catch {
		return []
	}
}
