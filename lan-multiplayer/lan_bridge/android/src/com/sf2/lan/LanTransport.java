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
import java.security.SecureRandom;
import java.util.Arrays;
import java.util.Locale;
import java.util.Properties;
import java.util.Random;
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
    private static final int TYPE_PROFILE = 7;
    private static final int TYPE_READY = 8;
    private static final int TYPE_FIGHT_READY = 9;
    private static final int TYPE_START = 10;
    private static final int TYPE_START_ACK = 11;
    private static final int TYPE_STATE = 12;
    private static final int TYPE_PLAYER_ROLE = 13;
    private static final int TYPE_FIGHT_CONFIG = 14;
    private static final int TYPE_FIGHT_AUTHORITY = 15;
    private static final int TYPE_MATCH_CONFIG = 16;
    private static final int TYPE_MATCH_CONFIG_ACK = 17;
    private static final int HEADER_SIZE = 36;
    private static final int INPUT_SIZE = 10;
    private static final int START_SIZE = 8;
    private static final int ROUND_ID_SIZE = 4;
    private static final int STATE_SIZE = 40;
    private static final int FIGHT_AUTHORITY_SIZE = 48;
    private static final int MATCH_CONFIG_ACK_SIZE = 8;
    private static final int KNOWN_BUTTONS_MASK = 0x3f;
    private static final int MAX_PAYLOAD = 1024;
    private static final long INPUT_INTERVAL_MS = 33;
    private static final long FIGHT_READY_INTERVAL_MS = 1000;
    private static final long FIGHT_CONFIG_INTERVAL_MS = 1000;
    private static final long MATCH_CONFIG_INTERVAL_MS = 1000;
    private static final long START_RETRY_INTERVAL_MS = 250;
    private static final long STATE_INTERVAL_MS = 50;
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
    private final int fightStartDelayMs;
    private final LanFighterProfile localProfile;
    private volatile LanFighterProfile remoteProfile;
    private LanFighterProfile localMatchProfile;
    private LanFighterProfile remoteMatchProfile;
    private boolean localReady;
    private boolean remoteReady;
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
    private long lastProfileSend;
    private long lastReadySend;
    private int lastFightGeneration;
    private boolean localFightReady;
    private boolean remoteFightReady;
    private boolean fightStartScheduled;
    private boolean fightStartAcked;
    private int activeRoundId;
    private long scheduledStartAtMs;
    private long lastFightReadySend;
    private long lastStartSend;
    private long lastStateSend;
    private LanFightState localFightState = LanFightState.EMPTY;
    private LanFightState remoteFightState = LanFightState.EMPTY;
    private LanFightAuthority localFightAuthority = LanFightAuthority.EMPTY;
    private LanFightAuthority remoteFightAuthority = LanFightAuthority.EMPTY;
    private int localPlayerRole;
    private LanFightConfig localFightConfig;
    private LanFightConfig remoteFightConfig;
    private long lastFightConfigSend;
    private LanMatchConfig matchConfig;
    private boolean matchConfigApplied;
    private boolean matchConfigConfirmed;
    private long lastMatchConfigSend;

    private LanTransport(Context context, Properties properties) {
        this.context = context;
        this.mode = properties.getProperty("mode", "off").trim().toLowerCase(Locale.ROOT);
        this.peerHost = properties.getProperty("peer", "").trim();
        this.port = Integer.parseInt(properties.getProperty("port", "24190").trim());
        this.token = properties.getProperty("token", "").trim();
        this.deviceName = properties.getProperty("name", android.os.Build.MODEL).getBytes(StandardCharsets.UTF_8);
        this.controllerMode = Boolean.parseBoolean(properties.getProperty("controller", "false").trim());
        this.nativeControls = Boolean.parseBoolean(properties.getProperty("nativeControls", "false").trim());
        this.fightStartDelayMs = Integer.parseInt(properties.getProperty("startDelayMs", "2000").trim());
        // No modo direto, host/client ja definem P1/P2. Em relay, PLAYER_ROLE
        // substitui este valor assim que o HELLO for aceito.
        this.localPlayerRole = mode.equals("host") ? 1 : 2;
        LanNativeBridge.setPlayerRole(this.localPlayerRole);
        LanFighterProfile profile = null;
        try {
            profile = LanFighterProfile.read(context);
        } catch (Exception error) {
            Log.w(TAG, "Nao foi possivel ler o perfil local para a sessao LAN.", error);
        }
        this.localProfile = profile;
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
        if (fightStartDelayMs < 500 || fightStartDelayMs > 10_000) {
            throw new IllegalArgumentException("startDelayMs deve ficar entre 500 e 10000");
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
                updateMatchHandshake(now);
                updateFightHandshake(now);
                if (mode.equals("client") && !connected && now - lastSend >= 1000) {
                    send(TYPE_HELLO, deviceName, peer);
                } else if (connected && !remoteReady && localProfile != null
                    && now - lastProfileSend >= 1000) {
                    sendLocalProfile();
                } else if (connected && localReady && !remoteReady
                    && now - lastReadySend >= 1000) {
                    sendReady();
                } else if (connected && isSessionReady() && sendsLocalInput() && peer != null
                    && now - lastInputSend >= INPUT_INTERVAL_MS) {
                    if (nativeControls) {
                        LanInputState local = LanNativeBridge.readLocalInput();
                        setLocalInput(local.horizontal, local.vertical, local.buttons);
                    }
                    send(TYPE_INPUT, encodeLocalInput(), peer);
                    lastInputSend = now;
                } else if (connected && isSessionReady() && !controllerMode
                    && activeRoundId != 0 && peer != null
                    && now - lastStateSend >= STATE_INTERVAL_MS) {
                    sendFightState(now);
                } else if (connected && peer != null && now - lastSend >= 1000) {
                    send(TYPE_PING, new byte[0], peer);
                }
                if (connected && now - lastReceive > 5000) {
                    connected = false;
                    resetSession();
                    writeStatus("timeout", peer, "heartbeat expirado");
                    if (mode.equals("client")) {
                        peer = new InetSocketAddress(InetAddress.getByName(peerHost), port);
                    } else {
                        peer = null;
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
            resetSession();
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
            sendLocalProfile();
            writeStatus("connected", peer, new String(payload, StandardCharsets.UTF_8));
        } else if (type == TYPE_WELCOME && mode.equals("client")) {
            connected = true;
            sendLocalProfile();
            writeStatus("connected", peer, new String(payload, StandardCharsets.UTF_8));
        } else if (type == TYPE_PING) {
            send(TYPE_PONG, new byte[0], peer);
        } else if (type == TYPE_PONG) {
            writeStatus("connected", peer, "heartbeat");
        } else if (type == TYPE_INPUT) {
            handleInput(payload, packetSequence);
        } else if (type == TYPE_PROFILE) {
            handleProfile(payload);
        } else if (type == TYPE_READY) {
            handleReady(payload);
        } else if (type == TYPE_FIGHT_READY) {
            handleFightReady(payload);
        } else if (type == TYPE_START) {
            handleStart(payload);
        } else if (type == TYPE_START_ACK) {
            handleStartAck(payload);
        } else if (type == TYPE_STATE) {
            handleFightState(payload);
        } else if (type == TYPE_PLAYER_ROLE) {
            handlePlayerRole(payload);
        } else if (type == TYPE_FIGHT_CONFIG) {
            handleFightConfig(payload);
        } else if (type == TYPE_FIGHT_AUTHORITY) {
            handleFightAuthority(payload);
        } else if (type == TYPE_MATCH_CONFIG) {
            handleMatchConfig(payload);
        } else if (type == TYPE_MATCH_CONFIG_ACK) {
            handleMatchConfigAck(payload);
        } else if (type == TYPE_DISCONNECT) {
            connected = false;
            resetSession();
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
        if (!connected || !isSessionReady() || payload.length != INPUT_SIZE) {
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

    private void sendLocalProfile() throws Exception {
        if (localProfile != null && peer != null) {
            send(TYPE_PROFILE, localProfile.encode(), peer);
            lastProfileSend = System.currentTimeMillis();
        }
    }

    private void handleProfile(byte[] payload) throws Exception {
        try {
            remoteProfile = LanFighterProfile.decode(payload);
            localReady = true;
            ensureHostMatchConfig();
            applyMatchConfigIfPossible();
            sendReady();
            publishSessionState();
            writeStatus("connected", peer, "perfil remoto recebido");
        } catch (IllegalArgumentException error) {
            Log.w(TAG, "Perfil remoto rejeitado.", error);
        }
    }

    private void sendReady() throws Exception {
        if (peer != null) {
            send(TYPE_READY, new byte[0], peer);
            lastReadySend = System.currentTimeMillis();
        }
    }

    private void handleReady(byte[] payload) {
        if (payload.length != 0) {
            Log.w(TAG, "READY rejeitado: payload deve ser vazio.");
            return;
        }
        remoteReady = true;
        publishSessionState();
        writeStatus("connected", peer, isSessionReady() ? "sessao pronta" : "peer pronto");
    }

    private void ensureHostMatchConfig() throws Exception {
        if (!mode.equals("host") || matchConfig != null || localProfile == null || remoteProfile == null) {
            return;
        }
        long seed;
        do {
            seed = new SecureRandom().nextLong();
        } while (seed == 0);
        Random random = new Random(seed);
        String playerOne = LanEquipmentPreset.NAMES[random.nextInt(LanEquipmentPreset.NAMES.length)];
        String playerTwo = LanEquipmentPreset.NAMES[random.nextInt(LanEquipmentPreset.NAMES.length)];
        int matchLevel = Math.max(1, Math.min(52, Math.min(localProfile.level, remoteProfile.level)));
        matchConfig = new LanMatchConfig(matchLevel, seed, playerOne, playerTwo);
        applyMatchConfigIfPossible();
        sendMatchConfig();
        Log.i(TAG, "Partida LAN sorteada pelo host: nivel=" + matchLevel
            + " P1=" + playerOne + " P2=" + playerTwo + ".");
    }

    private void updateMatchHandshake(long now) throws Exception {
        if (!connected || peer == null) {
            return;
        }
        ensureHostMatchConfig();
        applyMatchConfigIfPossible();
        if (matchConfig == null || !matchConfigApplied || matchConfigConfirmed
            || now - lastMatchConfigSend < MATCH_CONFIG_INTERVAL_MS) {
            return;
        }
        if (mode.equals("host")) {
            sendMatchConfig();
        } else {
            sendMatchConfigAck();
        }
    }

    private void handleMatchConfig(byte[] payload) throws Exception {
        LanMatchConfig received;
        try {
            received = LanMatchConfig.decode(payload);
        } catch (IllegalArgumentException error) {
            Log.w(TAG, "MATCH_CONFIG rejeitado.", error);
            return;
        }
        if (mode.equals("host")) {
            Log.w(TAG, "MATCH_CONFIG rejeitado: o host e a autoridade da partida.");
            return;
        }
        if (matchConfig != null && matchConfig.fingerprint() != received.fingerprint()) {
            Log.e(TAG, "MATCH_CONFIG divergente recebido durante a mesma sessao.");
            return;
        }
        matchConfig = received;
        applyMatchConfigIfPossible();
        if (matchConfigApplied) {
            sendMatchConfigAck();
        }
    }

    private void handleMatchConfigAck(byte[] payload) throws Exception {
        if (payload.length != MATCH_CONFIG_ACK_SIZE || matchConfig == null || !matchConfigApplied) {
            Log.w(TAG, "MATCH_CONFIG_ACK rejeitado.");
            return;
        }
        long fingerprint = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN).getLong();
        if (fingerprint == 0 || fingerprint != matchConfig.fingerprint()) {
            Log.w(TAG, "MATCH_CONFIG_ACK com fingerprint desconhecido.");
            return;
        }
        matchConfigConfirmed = true;
        if (mode.equals("host")) {
            send(TYPE_MATCH_CONFIG_ACK, payload, peer);
        }
        publishSessionState();
        writeStatus("connected", peer, "configuracao de partida confirmada");
        Log.i(TAG, "Partida LAN confirmada: " + matchConfig.toJson());
    }

    private void sendMatchConfig() throws Exception {
        if (peer != null && matchConfig != null) {
            send(TYPE_MATCH_CONFIG, matchConfig.encode(), peer);
            lastMatchConfigSend = System.currentTimeMillis();
        }
    }

    private void sendMatchConfigAck() throws Exception {
        if (peer != null && matchConfig != null) {
            byte[] payload = ByteBuffer.allocate(MATCH_CONFIG_ACK_SIZE).order(ByteOrder.BIG_ENDIAN)
                .putLong(matchConfig.fingerprint()).array();
            send(TYPE_MATCH_CONFIG_ACK, payload, peer);
            lastMatchConfigSend = System.currentTimeMillis();
        }
    }

    private void applyMatchConfigIfPossible() {
        if (matchConfigApplied || matchConfig == null || localProfile == null || remoteProfile == null) {
            return;
        }
        LanEquipmentPreset localPreset = LanEquipmentPreset.get(matchConfig.presetForRole(localPlayerRole));
        LanEquipmentPreset remotePreset = LanEquipmentPreset.get(matchConfig.remotePresetForRole(localPlayerRole));
        localMatchProfile = localProfile.withPreset(localPreset, matchConfig.matchLevel);
        remoteMatchProfile = remoteProfile.withPreset(remotePreset, matchConfig.matchLevel);
        LanNativeBridge.setMatchProfiles(localMatchProfile, remoteMatchProfile);
        matchConfigApplied = true;
        publishSessionState();
    }

    private void updateFightHandshake(long now) throws Exception {
        if (!connected || !isSessionReady() || controllerMode) {
            return;
        }
        int generation = LanNativeBridge.getFightGeneration();
        if (generation > 0 && generation != lastFightGeneration) {
            lastFightGeneration = generation;
            localFightReady = true;
            localFightConfig = LanNativeBridge.readFightConfig();
            remoteFightConfig = null;
            remoteFightReady = false;
            fightStartScheduled = false;
            fightStartAcked = false;
            activeRoundId = 0;
            scheduledStartAtMs = 0;
            lastStartSend = 0;
            lastStateSend = 0;
            localFightState = LanFightState.EMPTY;
            remoteFightState = LanFightState.EMPTY;
            localFightAuthority = LanFightAuthority.EMPTY;
            remoteFightAuthority = LanFightAuthority.EMPTY;
            LanNativeBridge.clearAuthoritativeFightState();
            lastFightConfigSend = 0;
            LanNativeBridge.updateRemoteFightClock(0.0f, 0);
            if (localFightConfig != null) {
                sendFightConfig();
                Log.i(TAG, "Arena LAN identificada: generation=" + generation
                    + " config=" + localFightConfig.identity + ".");
            } else {
                Log.e(TAG, "Arena LAN sem descritor; START bloqueado por seguranca.");
            }
        }
        if (!localFightReady) {
            return;
        }
        if (localFightConfig != null && remoteFightConfig == null
            && now - lastFightConfigSend >= FIGHT_CONFIG_INTERVAL_MS) {
            sendFightConfig();
        }
        if (fightConfigsMatch() && !fightStartScheduled
            && now - lastFightReadySend >= FIGHT_READY_INTERVAL_MS) {
            sendFightReady();
        }
        if (mode.equals("host") && remoteFightReady && !fightStartScheduled) {
            scheduleHostStart(now);
        } else if (mode.equals("host") && fightStartScheduled && !fightStartAcked
            && now < scheduledStartAtMs && now - lastStartSend >= START_RETRY_INTERVAL_MS) {
            sendStart();
        }
    }

    private void sendFightConfig() throws Exception {
        if (peer != null && localFightConfig != null) {
            send(TYPE_FIGHT_CONFIG, localFightConfig.encode(), peer);
            lastFightConfigSend = System.currentTimeMillis();
        }
    }

    private void handleFightConfig(byte[] payload) throws Exception {
        LanFightConfig previousRemoteConfig = remoteFightConfig;
        try {
            remoteFightConfig = LanFightConfig.decode(payload);
        } catch (IllegalArgumentException error) {
            Log.w(TAG, "FIGHT_CONFIG rejeitado.", error);
            return;
        }
        if (localFightConfig != null
            && (previousRemoteConfig == null || !previousRemoteConfig.sameFight(remoteFightConfig))) {
            // Funciona como confirmacao da configuracao. Isso cobre o caso em
            // que um peer entrou na arena antes de o outro e a primeira copia
            // do datagrama foi recebida antes de existir um FightConfig local.
            sendFightConfig();
        }
        if (!fightConfigsMatch()) {
            remoteFightReady = false;
            fightStartScheduled = false;
            fightStartAcked = false;
            activeRoundId = 0;
            scheduledStartAtMs = 0;
            Log.e(TAG, "Arenas divergentes; START bloqueado. local="
                + (localFightConfig == null ? "ausente" : localFightConfig.identity)
                + " remoto=" + remoteFightConfig.identity + ".");
            writeStatus("arena-mismatch", peer, "as duas instancias carregaram lutas diferentes");
            return;
        }
        sendFightReady();
        Log.i(TAG, "Configuracao da arena confirmada pelos dois jogadores: "
            + localFightConfig.identity + ".");
    }

    private boolean fightConfigsMatch() {
        return localFightConfig != null && localFightConfig.sameFight(remoteFightConfig);
    }

    private void handlePlayerRole(byte[] payload) {
        if (payload.length != 1 || (payload[0] != 1 && payload[0] != 2)) {
            Log.w(TAG, "PLAYER_ROLE rejeitado.");
            return;
        }
        localPlayerRole = payload[0];
        LanNativeBridge.setPlayerRole(localPlayerRole);
        matchConfigApplied = false;
        applyMatchConfigIfPossible();
        Log.i(TAG, "Identidade LAN atribuida pelo relay: jogador " + localPlayerRole + ".");
        writeStatus("connected", peer, "jogador " + localPlayerRole);
    }

    private void sendFightReady() throws Exception {
        if (peer != null) {
            send(TYPE_FIGHT_READY, new byte[0], peer);
            lastFightReadySend = System.currentTimeMillis();
        }
    }

    private void handleFightReady(byte[] payload) throws Exception {
        if (!isSessionReady() || !fightConfigsMatch() || payload.length != 0) {
            Log.w(TAG, "FIGHT_READY rejeitado: sessao ou payload invalido.");
            return;
        }
        remoteFightReady = true;
        Log.i(TAG, "Peer confirmou que a arena LAN foi carregada.");
        if (mode.equals("host") && localFightReady && !fightStartScheduled) {
            scheduleHostStart(System.currentTimeMillis());
        }
    }

    private void scheduleHostStart(long now) throws Exception {
        activeRoundId = ((int) now) & 0x7fffffff;
        if (activeRoundId == 0) {
            activeRoundId = 1;
        }
        scheduledStartAtMs = now + fightStartDelayMs;
        fightStartScheduled = true;
        fightStartAcked = false;
        LanNativeBridge.scheduleFightStart(scheduledStartAtMs);
        sendStart();
        Log.i(TAG, "Inicio LAN agendado pelo host: round=" + activeRoundId
            + " delay=" + fightStartDelayMs + "ms.");
    }

    private void sendStart() throws Exception {
        if (peer == null || activeRoundId == 0) {
            return;
        }
        byte[] payload = ByteBuffer.allocate(START_SIZE).order(ByteOrder.BIG_ENDIAN)
            .putInt(activeRoundId)
            .putInt(fightStartDelayMs)
            .array();
        send(TYPE_START, payload, peer);
        lastStartSend = System.currentTimeMillis();
    }

    private void handleStart(byte[] payload) throws Exception {
        if (!mode.equals("client") || !localFightReady || payload.length != START_SIZE) {
            Log.w(TAG, "START rejeitado: modo, arena ou payload invalido.");
            return;
        }
        ByteBuffer start = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN);
        int roundId = start.getInt();
        int delayMs = start.getInt();
        if (roundId == 0 || delayMs < 500 || delayMs > 10_000) {
            Log.w(TAG, "START rejeitado: valores fora do protocolo.");
            return;
        }
        if (!fightStartScheduled || activeRoundId != roundId) {
            activeRoundId = roundId;
            scheduledStartAtMs = System.currentTimeMillis() + delayMs;
            fightStartScheduled = true;
            LanNativeBridge.scheduleFightStart(scheduledStartAtMs);
            Log.i(TAG, "Inicio LAN recebido: round=" + activeRoundId
                + " delay=" + delayMs + "ms.");
        }
        // O relay so emite START depois que recebeu FIGHT_READY dos dois lados.
        // Registrar isso localmente evita uma falsa indicacao de peer ausente
        // quando o FIGHT_READY chegou durante a troca de geracao da arena.
        remoteFightReady = true;
        sendStartAck(roundId);
        fightStartAcked = true;
    }

    private void sendStartAck(int roundId) throws Exception {
        if (peer != null) {
            byte[] payload = ByteBuffer.allocate(ROUND_ID_SIZE).order(ByteOrder.BIG_ENDIAN)
                .putInt(roundId)
                .array();
            send(TYPE_START_ACK, payload, peer);
        }
    }

    private void handleStartAck(byte[] payload) {
        if (!mode.equals("host") || payload.length != ROUND_ID_SIZE) {
            Log.w(TAG, "START_ACK rejeitado: modo ou payload invalido.");
            return;
        }
        int roundId = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN).getInt();
        if (!fightStartScheduled || roundId != activeRoundId) {
            Log.w(TAG, "START_ACK rejeitado: rodada desconhecida.");
            return;
        }
        fightStartAcked = true;
        Log.i(TAG, "Inicio LAN confirmado pelo peer: round=" + roundId + ".");
    }

    private void sendFightState(long now) throws Exception {
        lastStateSend = now;
        localFightState = LanNativeBridge.readLocalFightState(activeRoundId & 0xffffffffL);
        if (!localFightState.isAvailable() || peer == null) {
            return;
        }
        ByteBuffer payload = ByteBuffer.allocate(STATE_SIZE).order(ByteOrder.BIG_ENDIAN)
            .putInt((int) localFightState.roundId)
            .putInt((int) localFightState.tick);
        for (float value : localFightState.values) {
            payload.putFloat(value);
        }
        send(TYPE_STATE, payload.array(), peer);
        localFightAuthority = LanNativeBridge.readLocalFightAuthority(
            activeRoundId & 0xffffffffL
        );
        if (localPlayerRole == 1) {
            if (localFightAuthority.isAvailable()) {
                ByteBuffer authority = ByteBuffer.allocate(FIGHT_AUTHORITY_SIZE)
                    .order(ByteOrder.BIG_ENDIAN)
                    .putInt((int) localFightAuthority.roundId)
                    .putInt((int) localFightAuthority.tick)
                    .putFloat(localFightAuthority.playerDamage)
                    .putFloat(localFightAuthority.opponentDamage)
                    .putInt(localFightAuthority.playerRoundsLost)
                    .putInt(localFightAuthority.opponentRoundsLost)
                    .putFloat(localFightAuthority.playerX)
                    .putFloat(localFightAuthority.playerY)
                    .putFloat(localFightAuthority.playerZ)
                    .putFloat(localFightAuthority.opponentX)
                    .putFloat(localFightAuthority.opponentY)
                    .putFloat(localFightAuthority.opponentZ);
                send(TYPE_FIGHT_AUTHORITY, authority.array(), peer);
            }
        }
    }

    private void handleFightState(byte[] payload) {
        if (controllerMode || !isSessionReady() || payload.length != STATE_SIZE) {
            Log.w(TAG, "STATE rejeitado: estado ou tamanho invalido.");
            return;
        }
        ByteBuffer state = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN);
        long roundId = state.getInt() & 0xffffffffL;
        long tick = state.getInt() & 0xffffffffL;
        if (roundId == 0 || tick == 0 || roundId != (activeRoundId & 0xffffffffL)) {
            return;
        }
        float[] values = new float[8];
        for (int index = 0; index < values.length; index++) {
            values[index] = state.getFloat();
            if (!Float.isFinite(values[index]) || Math.abs(values[index]) > 1_000_000.0f) {
                Log.w(TAG, "STATE rejeitado: float invalido.");
                return;
            }
        }
        if (remoteFightState.roundId == roundId
            && !isNewerSequence(tick, remoteFightState.tick)) {
            return;
        }
        remoteFightState = new LanFightState(roundId, tick, values);
        LanNativeBridge.updateRemoteFightClock(values[6], remoteFightState.capturedAtMs);
    }

    private void handleFightAuthority(byte[] payload) {
        if (controllerMode || localPlayerRole != 2 || !isSessionReady()
            || payload.length != FIGHT_AUTHORITY_SIZE) {
            Log.w(TAG, "FIGHT_AUTHORITY rejeitado: papel, estado ou tamanho invalido.");
            return;
        }
        ByteBuffer state = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN);
        long roundId = state.getInt() & 0xffffffffL;
        long tick = state.getInt() & 0xffffffffL;
        float playerDamage = state.getFloat();
        float opponentDamage = state.getFloat();
        int playerRoundsLost = state.getInt();
        int opponentRoundsLost = state.getInt();
        float playerX = state.getFloat();
        float playerY = state.getFloat();
        float playerZ = state.getFloat();
        float opponentX = state.getFloat();
        float opponentY = state.getFloat();
        float opponentZ = state.getFloat();
        if (roundId == 0 || tick == 0 || roundId != (activeRoundId & 0xffffffffL)
            || !Float.isFinite(playerDamage) || playerDamage < 0.0f || playerDamage > 1.0f
            || !Float.isFinite(opponentDamage) || opponentDamage < 0.0f || opponentDamage > 1.0f
            || playerRoundsLost < 0 || playerRoundsLost > 2
            || opponentRoundsLost < 0 || opponentRoundsLost > 2
            || !validCoordinate(playerX) || !validCoordinate(playerY) || !validCoordinate(playerZ)
            || !validCoordinate(opponentX) || !validCoordinate(opponentY)
            || !validCoordinate(opponentZ)) {
            Log.w(TAG, "FIGHT_AUTHORITY rejeitado: valores invalidos.");
            return;
        }
        if (remoteFightAuthority.roundId == roundId
            && !isNewerSequence(tick, remoteFightAuthority.tick)) {
            return;
        }
        remoteFightAuthority = new LanFightAuthority(
            roundId, tick, playerDamage, opponentDamage,
            playerRoundsLost, opponentRoundsLost,
            playerX, playerY, playerZ, opponentX, opponentY, opponentZ
        );
        LanNativeBridge.updateAuthoritativeFightState(remoteFightAuthority);
    }

    private void resetSession() {
        localReady = false;
        remoteReady = false;
        remoteProfile = null;
        localMatchProfile = null;
        remoteMatchProfile = null;
        LanNativeBridge.setMatchProfiles(null, null);
        matchConfig = null;
        matchConfigApplied = false;
        matchConfigConfirmed = false;
        lastMatchConfigSend = 0;
        lastProfileSend = 0;
        lastReadySend = 0;
        lastFightGeneration = 0;
        localFightReady = false;
        remoteFightReady = false;
        fightStartScheduled = false;
        fightStartAcked = false;
        activeRoundId = 0;
        scheduledStartAtMs = 0;
        lastFightReadySend = 0;
        lastStartSend = 0;
        lastStateSend = 0;
        localFightState = LanFightState.EMPTY;
        remoteFightState = LanFightState.EMPTY;
        localFightAuthority = LanFightAuthority.EMPTY;
        remoteFightAuthority = LanFightAuthority.EMPTY;
        LanNativeBridge.clearAuthoritativeFightState();
        localFightConfig = null;
        remoteFightConfig = null;
        lastFightConfigSend = 0;
        LanNativeBridge.updateRemoteFightClock(0.0f, 0);
        remoteInput = LanInputState.NEUTRAL;
        LanNativeBridge.setSessionReady(false);
        if (!controllerMode) {
            LanNativeBridge.update(remoteInput);
        }
    }

    private boolean isSessionReady() {
        return localReady && remoteReady && matchConfigApplied && matchConfigConfirmed;
    }

    private void publishSessionState() {
        LanNativeBridge.setSessionReady(isSessionReady());
    }

    private boolean sendsLocalInput() {
        return controllerMode || nativeControls;
    }

    private static boolean isNewerSequence(long candidate, long previous) {
        long difference = (candidate - previous) & 0xffffffffL;
        return difference != 0 && difference < 0x80000000L;
    }

    private static boolean validCoordinate(float value) {
        return Float.isFinite(value) && Math.abs(value) <= 1_000_000.0f;
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
            long statusTime = System.currentTimeMillis();
            LanFightStateComparison comparison = LanFightStateComparison.compare(
                localFightState, remoteFightState, statusTime
            );
            String json = String.format(
                Locale.ROOT,
                "{\"state\":\"%s\",\"mode\":\"%s\",\"controller\":%s,\"nativeControls\":%s,\"peer\":\"%s\",\"detail\":\"%s\",\"nativeHook\":%s,"
                    + "\"input\":{\"horizontal\":%.6f,\"vertical\":%.6f,\"buttons\":%d,\"sequence\":%d,\"receivedAt\":%d},"
                    + "\"localInput\":{\"horizontal\":%.6f,\"vertical\":%.6f,\"buttons\":%d},"
                    + "\"localProfile\":%s,\"remoteProfile\":%s,\"localMatchProfile\":%s,\"remoteMatchProfile\":%s,\"matchConfig\":%s,\"matchConfigApplied\":%s,\"matchConfigConfirmed\":%s,\"localReady\":%s,\"remoteReady\":%s,\"sessionReady\":%s,"
                    + "\"fight\":{\"generation\":%d,\"playerRole\":%d,\"localReady\":%s,\"remoteReady\":%s,\"configMatch\":%s,\"localConfig\":%s,\"remoteConfig\":%s,\"startScheduled\":%s,\"startAcked\":%s,\"roundId\":%d,\"startAt\":%d},"
                    + "\"fightState\":{\"local\":%s,\"remote\":%s,\"comparison\":%s},"
                    + "\"fightAuthority\":{\"local\":%s,\"remote\":%s},"
                    + "\"updated\":%d}\n",
                escape(state), escape(mode), controllerMode, nativeControls,
                escape(peerText), escape(detail), LanNativeBridge.isInstalled(),
                remoteInput.horizontal, remoteInput.vertical, remoteInput.buttons,
                remoteInput.sequence, remoteInput.receivedAtMs,
                localHorizontal, localVertical, localButtons,
                localProfile == null ? "null" : localProfile.toJson(),
                remoteProfile == null ? "null" : remoteProfile.toJson(),
                localMatchProfile == null ? "null" : localMatchProfile.toJson(),
                remoteMatchProfile == null ? "null" : remoteMatchProfile.toJson(),
                matchConfig == null ? "null" : matchConfig.toJson(),
                matchConfigApplied, matchConfigConfirmed,
                localReady, remoteReady, isSessionReady(),
                LanNativeBridge.getFightGeneration(), localPlayerRole, localFightReady, remoteFightReady,
                fightConfigsMatch(),
                localFightConfig == null ? "null" : localFightConfig.toJson(),
                remoteFightConfig == null ? "null" : remoteFightConfig.toJson(),
                fightStartScheduled, fightStartAcked, activeRoundId, scheduledStartAtMs,
                localFightState.isAvailable() ? localFightState.toJson() : "null",
                remoteFightState.isAvailable() ? remoteFightState.toJson() : "null",
                comparison.isAvailable() ? comparison.toJson() : "null",
                localFightAuthority.isAvailable() ? localFightAuthority.toJson() : "null",
                remoteFightAuthority.isAvailable() ? remoteFightAuthority.toJson() : "null",
                statusTime
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
