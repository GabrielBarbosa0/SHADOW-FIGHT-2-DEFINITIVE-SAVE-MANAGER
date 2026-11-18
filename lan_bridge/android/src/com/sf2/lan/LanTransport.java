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
    private static final int TYPE_DISCONNECT = 6;
    private static final int HEADER_SIZE = 36;
    private static final int MAX_PAYLOAD = 1024;
    private static final AtomicBoolean STARTED = new AtomicBoolean(false);

    private final Context context;
    private final String mode;
    private final String peerHost;
    private final int port;
    private final String token;
    private final byte[] deviceName;
    private DatagramSocket socket;
    private InetSocketAddress peer;
    private int sequence;
    private long lastSend;
    private long lastReceive;
    private boolean connected;

    private LanTransport(Context context, Properties properties) {
        this.context = context;
        this.mode = properties.getProperty("mode", "off").trim().toLowerCase(Locale.ROOT);
        this.peerHost = properties.getProperty("peer", "").trim();
        this.port = Integer.parseInt(properties.getProperty("port", "24190").trim());
        this.token = properties.getProperty("token", "").trim();
        this.deviceName = properties.getProperty("name", android.os.Build.MODEL).getBytes(StandardCharsets.UTF_8);
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
            Thread thread = new Thread(transport, "sf2-lan-udp");
            thread.setDaemon(true);
            thread.start();
        } catch (Exception error) {
            STARTED.set(false);
            Log.e(TAG, "Falha ao iniciar transporte LAN.", error);
        }
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
    }

    @Override
    public void run() {
        try {
            socket = mode.equals("host") ? new DatagramSocket(port) : new DatagramSocket();
            socket.setSoTimeout(250);
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
                } else if (connected && peer != null && now - lastSend >= 1000) {
                    send(TYPE_PING, new byte[0], peer);
                }
                if (connected && now - lastReceive > 5000) {
                    connected = false;
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
        buffer.getInt(); // sequence
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
        } else if (type == TYPE_DISCONNECT) {
            connected = false;
            writeStatus("disconnected", peer, "peer encerrou a sessao");
            if (mode.equals("host")) {
                peer = null;
            }
        }
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
                "{\"state\":\"%s\",\"mode\":\"%s\",\"peer\":\"%s\",\"detail\":\"%s\",\"updated\":%d}\n",
                escape(state), escape(mode), escape(peerText), escape(detail), System.currentTimeMillis()
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
