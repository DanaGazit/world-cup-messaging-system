package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import bgu.spl.net.impl.data.LoginStatus;
import bgu.spl.net.impl.data.Database;
public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {

    private int connectionId;//from server
    private Connections<String> connections;
    private boolean shouldTerminate = false;
    private Database db = Database.getInstance();
    // global: 

    private static final ConcurrentHashMap<String, String> usersMap = new ConcurrentHashMap<>(); //saves all client login&passcode
    private static final ConcurrentHashMap<String, Boolean> activeUsersMap = new ConcurrentHashMap<>(); //saves only clients that are connected now
    private static final ConcurrentHashMap<String, ConcurrentHashMap<Integer, String>> topicsMapForAll = new ConcurrentHashMap<>(); //channelName-> all the sub(id connection -> id sub)
    // specific client :
    private String currentUser = null; // user name, defaults to null until login
    private ConcurrentHashMap<String, String> subscriptionIdToChannel = new ConcurrentHashMap<>(); //client's map subId -> channel for msg reply, subID can be String

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @Override
    public void process(String message){
        String[] lines = message.split("\n"); //spliting lines
        if (lines.length == 0) //empty msg
            return;
        String command = lines[0].trim(); 
        if (command.equals("CONNECT")){
            handleConnect(lines);
        } 
        else if(command.equals("SUBSCRIBE")) {
             handleSubscribe(lines);
        } 
        else if(command.equals("UNSUBSCRIBE")) {
             handleUnsubscribe(lines);
        } 
        else if (command.equals("DISCONNECT")) {
            handleDisconnect(lines);
        } 
        else if (command.equals("SEND")) {
            handleSend(lines);
        } 
        else {
            connections.send(connectionId, createErrorFrame("Unknown command", "didnt recognize command " + command,null));
        }
    }

    @Override
    public boolean shouldTerminate(){
        return shouldTerminate;
    }

    private void handleConnect(String[] lines) {
        String login = null;
        String passcode = null;
        String version = null;
        String host = null;
        String receipt = null;
        int i = 1;
        for (; i < lines.length; i++){
            String line = lines[i].trim();
            if (line.isEmpty()) 
                break; //we arrived to the "body"

            String[] parts = line.split(":", 2); //2 sections
            String key = parts[0].trim();
            String value = parts[1].trim();
            if (key.equals("login")) {
                login = value;
            } 
            else if (key.equals("passcode")) {
                passcode = value;
            } 
            else if (key.equals("accept-version")) {
                version = value;
            } 
            else if (key.equals("host")) {
                host = value;
            }
            else if (key.equals("receipt"))
                receipt = value;
        }

        if (version == null || !version.equals("1.2")){
            connections.send(connectionId, createErrorFrame("VERSION error", "supported version is 1.2",receipt));
            return;
        }
        if (host == null|| !host.equals("stomp.cs.bgu.ac.il")) {
            connections.send(connectionId, createErrorFrame("HOST error", "host must be stomp.cs.bgu.ac.il",receipt));
            return;
        }
        if (login == null|| passcode == null) {
            connections.send(connectionId, createErrorFrame("malformed frame", "missing login or passcode",receipt));
            return;
        }
        if(!emptyBody(i,lines)){
            connections.send(connectionId, createErrorFrame("malformed frame", " <FrameBody> must be empty",receipt));
            return;
        }

        synchronized (usersMap) {
            LoginStatus status = db.login(connectionId, login, passcode);
            if (status == LoginStatus.LOGGED_IN_SUCCESSFULLY || status == LoginStatus.ADDED_NEW_USER) {
                currentUser = login;
                activeUsersMap.put(login, true);
                usersMap.put(login, passcode); 
                connections.send(connectionId, createConnectedFrame());
            } 
            else if (status == LoginStatus.WRONG_PASSWORD) {
                connections.send(connectionId, createErrorFrame("login failed", "Wrong password", null));
            } 
            else if (status == LoginStatus.ALREADY_LOGGED_IN) {
                connections.send(connectionId, createErrorFrame("login failed", "User already logged in", null));
            } 
            else if (status == LoginStatus.CLIENT_ALREADY_CONNECTED) {
                 connections.send(connectionId, createErrorFrame("login failed", "Client already connected", null));
            }
        }
    }

    private void handleSubscribe(String[] lines) {
        String destination = null;
        String idChannel = null;
        String receipt = null;
        int i=1;

        for (; i < lines.length; i++){
            String line = lines[i].trim();
            if (line.isEmpty()) 
                break;

            String[] parts = line.split(":", 2);
            String key = parts[0].trim();
            String value = parts[1].trim();
                
            if (key.equals("destination"))
                destination = value;
            if (key.equals("id"))
                idChannel = value;
            if (key.equals("receipt"))
                receipt = value;
            
        }
        if(!emptyBody(i,lines))
            return;

        if (destination == null || idChannel == null) {
            connections.send(connectionId, createErrorFrame("malformed frame", "missing dest or id headers",receipt));
            return;
        }
        if(!isActive(currentUser)){
            connections.send(connectionId, createErrorFrame("Unconnected user", "user is not logged in",receipt));
            return;
        }
        if(subscriptionIdToChannel.containsKey(idChannel)){
            connections.send(connectionId, createErrorFrame("Subscription error", "subscription ID already exists", receipt));
            return;
        }
        connections.subscribe(destination, connectionId);
        subscriptionIdToChannel.put(idChannel, destination);
        
        topicsMapForAll.computeIfAbsent(destination, k -> new ConcurrentHashMap<>())
                       .put(connectionId, idChannel);

        if (receipt != null)
            connections.send(connectionId, createReceiptFrame(receipt));
    }

    private void handleUnsubscribe(String[] lines) {
        String id = null;
        String receipt = null;
        int i = 1;
        for (; i < lines.length; i++) {
            String line = lines[i].trim();
            if (line.isEmpty()) 
                break;

            String[] parts = line.split(":", 2);
            String key = parts[0].trim();
            String value = parts[1].trim();
            if (key.equals("id")) id = value;
            if (key.equals("receipt")) receipt = value;
            
        }
        if(!emptyBody(i,lines))
            return;

        if (id == null){
            connections.send(connectionId, createErrorFrame("malformed frame", "missing id header",receipt));
            return;
        }
         if(!isActive(currentUser)){
            connections.send(connectionId, createErrorFrame("Unconnected user", "user is not logged in",receipt));
            return;
        }
        String channel = subscriptionIdToChannel.remove(id);
        if (channel != null){ //unSub case
            connections.unsubscribe(channel, connectionId);
            
            ConcurrentHashMap<Integer, String> subs = topicsMapForAll.get(channel);
            if (subs != null) {
                subs.remove(connectionId);
            }
        }
        else{
            connections.send(connectionId, createErrorFrame("Unsubscribed failed", "User is not subscribed to: " + id,receipt));
            return;
        }
        if (receipt != null)
            connections.send(connectionId, createReceiptFrame(receipt));
    }

    private void handleSend(String[] lines) {
        String destination = null;
        String receiptId = null; // save this to return receipt later
        String filename=null;
        int bodyStartIndex = 0;

        for (int i = 1; i < lines.length; i++) {
            String line = lines[i].trim();
            if (line.isEmpty()) {
                bodyStartIndex = i + 1; 
                break;
            }

            String[] parts = line.split(":", 2);
            String key = parts[0].trim();
            String value = parts[1].trim();
                
            if (key.equals("destination")) {
                destination = value;
             } 
            else if (key.equals("receipt")) { 
                receiptId = value; // catch receipt request
            }
            else if (key.equals("file")){
                 filename = value; 
            }
            
        }

        if (destination == null) {
            connections.send(connectionId, createErrorFrame("malformed frame", "no destination header",receiptId));
            return;
        }

        if(!isActive(currentUser)){ //user is disconnected
            connections.send(connectionId, createErrorFrame("User logged out", "user is disconnected",receiptId));
            return;
        }
        //client is not subscribed to the channel
        if(!subscriptionIdToChannel.containsValue(destination)){
            connections.send(connectionId, createErrorFrame("Unsubscribed user", "Unsubscribed users is not allowed to send msgs",receiptId));
            return;
        }
        //therefor someone is subscribed to the channel
        
        String body = "";
            for (int k = bodyStartIndex; k < lines.length; k++) {
                body = body + lines[k] + "\n";
            }
            body = body.replace("\u0000", "").trim();
        
        if (filename!=null) {
            db.trackFileUpload(currentUser, filename, destination);
        }
        ConcurrentHashMap<Integer, String> subscribers = topicsMapForAll.get(destination);
        if (subscribers != null) {
            for (Map.Entry<Integer, String> entry : subscribers.entrySet()){// sending to subscribed clients
                String messageFrame = createMessageFrame(destination, body, entry.getValue()); 
                connections.send(entry.getKey(), messageFrame);
            }
        }
        
        // send receipt only if requested
        if (receiptId != null) {
            connections.send(connectionId, createReceiptFrame(receiptId));
        }
    }

    private void handleDisconnect(String[] lines) {
        String receipt = null;
        int i = 2;
        if(lines.length<=1){
            connections.send(connectionId, createErrorFrame("malformed frame", "no receipt and id in msg",receipt));
            return;
        }
        String line = lines[1].trim();
        String[] parts= line.split(":", 2);
        String key = parts[0].trim();
        String value= parts[1].trim();
        if (key.equals("receipt")) receipt = value;
    
        if (!emptyBody(i, lines))
            return;
        if (receipt == null) {
            connections.send(connectionId, createErrorFrame("malformed frame", "The DISCONNECT frame MUST contain receipt id",receipt));
            return;
        }
        if (currentUser == null){ //never been connected
            connections.send(connectionId, createErrorFrame("Unknown user", "user has never been connected",receipt));
            return;
        }

        if (!activeUsersMap.get(currentUser)) {
            connections.send(connectionId, createErrorFrame("Disconnected user", "already disconnected",receipt));
            return;
        }
        disconnectFromChannel();
        db.logout(connectionId);
        synchronized(usersMap){//same lock as in the connect - solving case - when someone is trying to disconnect and then connect immediately it won't throw ERROR
             activeUsersMap.put(currentUser, false);
        }
        connections.send(connectionId, createReceiptFrame(receipt));
        connections.disconnect(connectionId);
        shouldTerminate=true;//gracefull shutdown
    }

    private String createConnectedFrame(){
        return "CONNECTED\n" +
               "version:1.2\n" +
               "\n" +
               '\u0000';
    }

    private String createMessageFrame(String destination, String body,String subscription) {
        return "MESSAGE\n" +
               "subscription:" + subscription + "\n" +
               "message-id:" + java.util.UUID.randomUUID().toString() + "\n" + //randomizer id 
               "destination:" + destination + "\n" +
               "\n" +
               body + "\n" +
               '\u0000';
    }

   private String createErrorFrame(String message, String description, String receiptId) {
    shouldTerminate = true; 
    if (receiptId == null) {
        return "ERROR\n" +
               "message:" + message + "\n" +
               "\n" +
               description + "\n" +
               '\u0000';
    } else {
        return "ERROR\n" +
               "receipt-id:" + receiptId + "\n" +
               "message:" + message + "\n" +
               "\n" +
               description + "\n" +
               '\u0000';
    }
}

    private String createReceiptFrame(String receiptId){
        return "RECEIPT\n" +
               "receipt-id:" + receiptId + "\n" +
               "\n" +
               '\u0000';
    }


    private boolean isActive(String name){
        return name!=null && activeUsersMap.get(name)!=null && activeUsersMap.get(name)==true;
    }

    private boolean emptyBody(int i, String [] lines){ //return true if empty
         for(;i<lines.length; i++){
            String line = lines[i];
            line=line.replace("\u0000", "").trim();
            if (!line.isEmpty()) {
                connections.send(connectionId, createErrorFrame("Malformed frame", "The body must be empty",null));
                return false;
            }
        } 
        return true;
    }

    private void disconnectFromChannel(){
        for(String Channel:subscriptionIdToChannel.values()){
            ConcurrentHashMap<Integer, String> subs = topicsMapForAll.get(Channel);
            if (subs != null) {
                subs.remove(connectionId);
            }
        }
    
    }


}