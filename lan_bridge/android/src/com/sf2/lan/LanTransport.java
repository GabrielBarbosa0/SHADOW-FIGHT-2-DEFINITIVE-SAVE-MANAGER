package com.sf2.lan;

import android.content.Context;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.Locale;
import java.util.Properties;
import java.util.concurrent.atomic.AtomicBoolean;

public final class LanTransport implements Runnable {
    private static final String TAG = "SF2LAN";
    private static final int MAGIC = 0x5346324c;
    private static final int VERSION = 1;
    private static final int TYPE_HELLO = 1;
    private static final int TYPE_WELCOME = 2;
    private static final int TYPE_PING = 3;
    private static final int TYPE_PONG = 4;
    private static final int TYPE_INPUT = 5;
    private static final int TYPE_DISCONNECT = 6;
    private static final int HEADER_SIZE = 36;
    private static final int INPUT_SIZE = 10;
    private static final int KNOWN_BUTTONS_MASK = 0x3f;
    private static final int MAX_PAYLOAD = 1024;
    private static final long INPUT_INTERVAL_MS = 33;
    private static final AtomicBoolean STARTED = new AtomicBoolean(false);
    private static volatile LanInputState remoteInput = LanInputState.NEUTRAL;
    private static volatile LanTransport activeTransport;

    private final Context context;
    private final String mode;
    private final String peerHost;
    private final int port;
    private final String token;
    private final byte[] deviceName;
    private final boolean controllerMode;
    private final boolean nativeControls;
    private DatagramSocket socket;
    private InetSocketAddress peer;
    private int sequence;
    private long lastSend;
    private long lastReceive;
    private boolean connected;
    private volatile float localHorizontal;
    private volatile float localVertical;
    private volatile int localButtons;
    private int pendingButtons;
    private long lastInputSend;

    private LanTransport(Context context, Properties properties) {
        this.context = context;
        this.mode = properties.getProperty("mode", "off").trim().toLowerCase(Locale.ROOT);
        this.peerHost = properties.getProperty("peer", "").trim();
        this.port = Integer.parseInt(properties.getProperty("port", "24190").trim());
        this.token = properties.getProperty("token", "").trim();
        this.deviceName = properties.getProperty("name", android.os.Build.MODEL).getBytes(StandardCharsets.UTF_8);
        this.controllerMode = Boolean.parseBoolean(properties.getProperty("controller", "false").trim());
        this.nativeControls = Boolean.parseBoolean(properties.getProperty("nativeControls", "false").trim());
    }

    public static void start(Context context) {
        if (!STARTED.compareAndSet(false, true)) {
            return;
        }
        try {
            File config = externalFile(context, "sf2lan.properties");
            if (!config.isFile()) {
                Log.i(TAG, "Transporte desativado: sf2lan.properties ausente.");
                STARTED.set(false);
                return;
            }
            Properties properties = new Properties();
            try (FileInputStream input = new FileInputStream(config)) {
                properties.load(input);
            }
            LanTransport transport = new LanTransport(context, properties);
            transport.validate();
            activeTransport = transport;
            if (transport.controllerMode) {
                LanControllerOverlay.start();
            } else {
                LanNativeBridge.start();
            }
            Thread thread = new Thread(transport, "sf2-lan-udp");
            thread.setDaemon(true);
            thread.start();
        } catch (Exception error) {
            STARTED.set(false);
            Log.e(TAG, "Falha ao iniciar transporte LAN.", error);
        }
    }

    /** Ponto estavel para a proxima etapa: o hook nativo consulta este snapshot. */
    public static LanInputState getRemoteInput() {
        return remoteInput;
    }

    static void updateLocalInput(float horizontal, float vertical, int buttons) {
        LanTransport transport = activeTransport;
        if (transport == null || !transport.controllerMode) {
            return;
        }
        transport.setLocalInput(horizontal, vertical, buttons);
    }

    private static float clamp(float value) {
        return Math.max(-1.0f, Math.min(1.0f, value));
    }

    private synchronized void setLocalInput(float horizontal, float vertical, int buttons) {
        int sanitizedButtons = buttons & KNOWN_BUTTONS_MASK;
        pendingButtons |= sanitizedButtons & ~localButtons;
        localHorizontal = clamp(horizontal);
        localVertical = clamp(vertical);
        localButtons = sanitizedButtons;
    }

    private void validate() {
        if (!mode.equals("host") && !mode.equals("client")) {
            throw new IllegalArgumentException("mode deve ser host ou client");
        }
        if (mode.equals("client") && peerHost.isEmpty()) {
            throw new IllegalArgumentException("peer e obrigatorio no modo client");
        }
        if (port < 1 || port > 65535) {
            throw new IllegalArgumentException("porta UDP invalida");
        }
        if (token.isEmpty() || token.getBytes(StandardCharsets.UTF_8).length > 128) {
            throw new IllegalArgumentException("token deve ter entre 1 e 128 bytes");
        }
        if (deviceName.length < 1 || deviceName.length > 64) {
            throw new IllegalArgumentException("name deve ter entre 1 e 64 bytes");
        }
        if (controllerMode && nativeControls) {
            throw new IllegalArgumentException("controller e nativeControls nao podem ser ativados juntos");
        }
    }

    @Override
    public void run() {
        try {
            socket = mode.equals("host") ? new DatagramSocket(port) : new DatagramSocket();
            socket.setSoTimeout(sendsLocalInput() ? 15 : 250);
            if (mode.equals("client")) {
                peer = new InetSocketAddress(InetAddress.getByName(peerHost), port);
            }
            writeStatus("waiting", peer, "");
            Log.i(TAG, "Transporte iniciado em modo " + mode + " na porta " + port + ".");

            byte[] receiveBuffer = new byte[HEADER_SIZE + MAX_PAYLOAD];
            while (!Thread.currentThread().isInterrupted()) {
                long now = System.currentTimeMillis();
                if (mode.equals("client") && !connected && now - lastSend >= 1000) {
                    send(TYPE_HELLO, deviceName, peer);
                } else if (connected && sendsLocalInput() && peer != null
                    && now - lastInputSend >= INPUT_INTERVAL_MS) {
                    if (nativeControls) {
                        LanInputState local = LanNativeBridge.readLocalInput();
                        setLocalInput(local.horizontal, local.vertical, local.buttons);
                    }
                    send(TYPE_INPUT, encodeLocalInput(), peer);
                    lastInputSend = now;
                } else if (connected && peer != null && now - lastSend >= 1000) {
                    send(TYPE_PING, new byte[0], peer);
                }
                if (connected && now - lastReceive > 5000) {
                    connected = false;
                    remoteInput = LanInputState.NEUTRAL;
                    if (!controllerMode) {
                        LanNativeBridge.update(remoteInput);
                    }
                    writeStatus("timeout", peer, "heartbeat expirado");
                    if (mode.equals("client")) {
                        peer = new InetSocketAddress(InetAddress.getByName(peerHost), port);
                    }
                }

                DatagramPacket datagram = new DatagramPacket(receiveBuffer, receiveBuffer.length);
                try {
                    socket.receive(datagram);
                    handle(datagram);
                } catch (SocketTimeoutException ignored) {
                    // O timeout curto permite enviar heartbeat e detectar desconexao.
                }
            }
        } catch (Exception error) {
            Log.e(TAG, "Transporte LAN encerrado por erro.", error);
            writeStatus("error", peer, error.toString());
        } finally {
            if (socket != null) {
                socket.close();
            }
            activeTransport = null;
            STARTED.set(false);
        }
    }

    private void handle(DatagramPacket datagram) throws Exception {
        InetSocketAddress sender = new InetSocketAddress(datagram.getAddress(), datagram.getPort());
        ByteBuffer buffer = ByteBuffer.wrap(datagram.getData(), datagram.getOffset(), datagram.getLength())
            .order(ByteOrder.BIG_ENDIAN);
        if (buffer.remaining() < HEADER_SIZE || buffer.getInt() != MAGIC) {
            return;
        }
        int version = buffer.get() & 0xff;
        int type = buffer.get() & 0xff;
        int payloadSize = buffer.getShort() & 0xffff;
        long packetSequence = buffer.getInt() & 0xffffffffL;
        buffer.getLong(); // timestamp
        byte[] receivedDigest = new byte[16];
        buffer.get(receivedDigest);
        if (version != VERSION || payloadSize > MAX_PAYLOAD || payloadSize != buffer.remaining()) {
            return;
        }
        if (!Arrays.equals(receivedDigest, tokenDigest())) {
            Log.w(TAG, "Datagrama rejeitado por token incorreto: " + sender);
            return;
        }
        if (mode.equals("host") && peer == null && type == TYPE_HELLO) {
            peer = sender;
        }
        if (peer == null || !peer.equals(sender)) {
            return;
        }
        lastReceive = System.currentTimeMillis();
        byte[] payload = new byte[payloadSize];
        buffer.get(payload);

        if (type == TYPE_HELLO && mode.equals("host")) {
            connected = true;
            send(TYPE_WELCOME, deviceName, peer);
            writeStatus("connected", peer, new String(payload, StandardCharsets.UTF_8));
        } else if (type == TYPE_WELCOME && mode.equals("client")) {
            connected = true;
            writeStatus("connected", peer, new String(payload, StandardCharsets.UTF_8));
        } else if (type == TYPE_PING) {
            send(TYPE_PONG, new byte[0], peer);
        } else if (type == TYPE_PONG) {
            writeStatus("connected", peer, "heartbeat");
        } else if (type == TYPE_INPUT) {
            handleInput(payload, packetSequence);
        } else if (type == TYPE_DISCONNECT) {
            connected = false;
            remoteInput = LanInputState.NEUTRAL;
            if (!controllerMode) {
                LanNativeBridge.update(remoteInput);
            }
            writeStatus("disconnected", peer, "peer encerrou a sessao");
            if (mode.equals("host")) {
                peer = null;
            }
        }
    }

    private void handleInput(byte[] payload, long packetSequence) {
        if (controllerMode) {
            return;
        }
        if (!connected || payload.length != INPUT_SIZE) {
            Log.w(TAG, "Pacote INPUT rejeitado: estado ou tamanho invalido.");
            return;
        }
        LanInputState previous = remoteInput;
        if (previous.sequence >= 0 && !isNewerSequence(packetSequence, previous.sequence)) {
            return;
        }
        ByteBuffer input = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN);
        float horizontal = input.getFloat();
        float vertical = input.getFloat();
        int buttons = input.getShort() & 0xffff;
        if (!Float.isFinite(horizontal) || !Float.isFinite(vertical)
            || horizontal < -1.0f || horizontal > 1.0f
            || vertical < -1.0f || vertical > 1.0f
            || (buttons & ~KNOWN_BUTTONS_MASK) != 0) {
            Log.w(TAG, "Pacote INPUT rejeitado: valores fora do protocolo.");
            return;
        }
        remoteInput = new LanInputState(horizontal, vertical, buttons, packetSequence, System.currentTimeMillis());
        LanNativeBridge.update(remoteInput);
        writeStatus("connected", peer, "input recebido");
    }

    private synchronized byte[] encodeLocalInput() {
        int outgoingButtons = localButtons | pendingButtons;
        pendingButtons = 0;
        return ByteBuffer.allocate(INPUT_SIZE).order(ByteOrder.BIG_ENDIAN)
            .putFloat(localHorizontal)
            .putFloat(localVertical)
            .putShort((short) outgoingButtons)
            .array();
    }

    private boolean sendsLocalInput() {
        return controllerMode || nativeControls;
    }

    private static boolean isNewerSequence(long candidate, long previous) {
        long difference = (candidate - previous) & 0xffffffffL;
        return difference != 0 && difference < 0x80000000L;
    }

    private void send(int type, byte[] payload, InetSocketAddress destination) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream(HEADER_SIZE + payload.length);
        try (DataOutputStream output = new DataOutputStream(bytes)) {
            output.writeInt(MAGIC);
            output.writeByte(VERSION);
            output.writeByte(type);
            output.writeShort(payload.length);
            output.writeInt(sequence++);
            output.writeLong(System.currentTimeMillis());
            output.write(tokenDigest());
            output.write(payload);
        }
        byte[] packet = bytes.toByteArray();
        socket.send(new DatagramPacket(packet, packet.length, destination));
        lastSend = System.currentTimeMillis();
    }

    private byte[] tokenDigest() throws Exception {
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(token.getBytes(StandardCharsets.UTF_8));
        return Arrays.copyOf(digest, 16);
    }

    private void writeStatus(String state, InetSocketAddress address, String detail) {
        try {
            File status = externalFile(context, "sf2lan-status.json");
            String peerText = address == null ? "" : address.getAddress().getHostAddress() + ":" + address.getPort();
            String json = String.format(
                Locale.ROOT,
                "{\"state\":\"%s\",\"mode\":\"%s\",\"controller\":%s,\"nativeControls\":%s,\"peer\":\"%s\",\"detail\":\"%s\",\"nativeHook\":%s,"
                    + "\"input\":{\"horizontal\":%.6f,\"vertical\":%.6f,\"buttons\":%d,\"sequence\":%d,\"receivedAt\":%d},"
                    + "\"localInput\":{\"horizontal\":%.6f,\"vertical\":%.6f,\"buttons\":%d},"
                    + "\"updated\":%d}\n",
                escape(state), escape(mode), controllerMode, nativeControls,
                escape(peerText), escape(detail), LanNativeBridge.isInstalled(),
                remoteInput.horizontal, remoteInput.vertical, remoteInput.buttons,
                remoteInput.sequence, remoteInput.receivedAtMs,
                localHorizontal, localVertical, localButtons, System.currentTimeMillis()
            );
            try (FileOutputStream output = new FileOutputStream(status, false)) {
                output.write(json.getBytes(StandardCharsets.UTF_8));
            }
        } catch (Exception error) {
            Log.w(TAG, "Nao foi possivel gravar status LAN.", error);
        }
    }

    private static String escape(String value) {
        return value.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n");
    }

    private static File externalFile(Context context, String name) {
        File root = context.getExternalFilesDir(null);
        if (root == null) {
            throw new IllegalStateException("diretorio externo do aplicativo indisponivel");
        }
        return new File(root, name);
    }
}
