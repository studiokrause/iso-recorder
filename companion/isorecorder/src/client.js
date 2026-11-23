import dgram from 'node:dgram'
import { EventEmitter } from 'node:events'
import { decode, encode } from './osc.js'

export const MAX_CHANNELS = 16

/**
 * UDP client of ISO Recorder's OSC interface. One socket both sends commands and receives the status:
 * the program answers to the address the command came from. Emits 'update' with a map {variableId: value},
 * 'online' (true/false) and 'error'.
 */
export class IsoRecorderClient extends EventEmitter {
	constructor() {
		super()
		this.socket = null
		this.timer = null
		this.host = '127.0.0.1'
		this.port = 8000
		this.lastSeen = 0
		this.online = false
		this.values = {}
	}

	/** @param {{host:string, port:number, localPort?:number}} cfg */
	async open(cfg) {
		this.close()
		this.host = cfg.host || '127.0.0.1'
		this.port = Number(cfg.port) || 8000
		this.values = {}
		const socket = dgram.createSocket('udp4')
		this.socket = socket
		socket.on('message', (msg) => this.#onMessage(msg))
		socket.on('error', (e) => this.emit('error', e))
		await new Promise((resolve, reject) => {
			socket.once('error', reject)
			socket.bind(Number(cfg.localPort) || 0, () => {
				socket.off('error', reject)
				resolve()
			})
		})
		this.send('/isorec/status')
		// keepalive: the program forgets clients that are silent for ~20 s; this also gives the full state again
		this.timer = setInterval(() => {
			this.send('/isorec/status')
			if (this.online && Date.now() - this.lastSeen > 12000) this.#setOnline(false)
		}, 5000)
	}

	close() {
		if (this.timer) clearInterval(this.timer)
		this.timer = null
		if (this.socket) {
			try {
				this.socket.close()
			} catch {
				/* already closed */
			}
		}
		this.socket = null
		this.#setOnline(false)
	}

	send(path, ...args) {
		if (!this.socket) return
		this.socket.send(encode(path, args), this.port, this.host, (err) => {
			if (err) this.emit('error', err)
		})
	}

	#setOnline(on) {
		if (this.online === on) return
		this.online = on
		this.emit('online', on)
	}

	#onMessage(msg) {
		const changed = {}
		for (const m of decode(msg)) {
			const key = variableId(m.path)
			if (!key || m.args.length === 0) continue
			const value = m.args[0]
			if (this.values[key] !== value) {
				this.values[key] = value
				changed[key] = value
			}
		}
		this.lastSeen = Date.now()
		this.#setOnline(true)
		if (Object.keys(changed).length) this.emit('update', changed)
	}
}

/** '/isorec/ch/3/state' -> 'ch3_state'; '/isorec/rec/count' -> 'rec_count'; null for other paths */
export function variableId(path) {
	const p = path.split('/').filter(Boolean)
	if (p[0] !== 'isorec') return null
	if (p[1] === 'ch' && p.length === 4) return `ch${p[2]}_${p[3]}`
	if (p.length === 2) return p[1]
	if (p.length === 3) return `${p[1]}_${p[2]}`
	return null
}
