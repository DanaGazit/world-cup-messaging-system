package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

// This class manages all the connections between server and clients

public class ConnectionsImpl <T> implements Connections<T> {
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> activeConnections = new ConcurrentHashMap<>(); //thread safe
    private final ConcurrentHashMap<String, CopyOnWriteArrayList<Integer>> channelsMap= new ConcurrentHashMap<>(); //thread safe

    @Override
    //sends a msg T to the client connectionId
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> clientCH = activeConnections.get(connectionId);
        if (clientCH==null)
            return false; //the user is not active, cant send msg

        clientCH.send(msg);
        return true;
    }

    @Override
    //sends a msg T to all the clients subscribed in the channel
    public void send(String channel, T msg) {
        CopyOnWriteArrayList<Integer> channelClients = channelsMap.get(channel);
        if(channelClients == null){
            //no such channel
            return;
        }
            
        for( Integer clientID : channelClients){
            send(clientID, msg);
        }
    }

    @Override
    //disconnects the client connectionId
    public void disconnect(int connectionId) {
        activeConnections.remove(connectionId);
        for(CopyOnWriteArrayList<Integer> subscribers : channelsMap.values()){
            subscribers.remove((Integer)connectionId);
        }
    }

       //connects the client
    public void connect(int connectionId, ConnectionHandler<T> ch){
        activeConnections.put(connectionId, ch); 
    }

    //subscribe an active client to a channel
    public void subscribe(String channel, int connectionId){
        CopyOnWriteArrayList<Integer> arr = channelsMap.computeIfAbsent(channel,l -> new CopyOnWriteArrayList<>()); //lambda needed for one automic step
        arr.addIfAbsent(connectionId); //add client to the channel if it doesn't exist
    }

    //unsubscribe an active client from a channel
    public void unsubscribe(String channel, int connectionId){
        CopyOnWriteArrayList<Integer> channelClients = channelsMap.get(channel);
        if(channelClients != null){
           channelClients.remove((Integer)connectionId); //safe removal
        }
    }

}
