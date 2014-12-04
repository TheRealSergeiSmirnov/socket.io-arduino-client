//Express initializes app to be a function handler that you can supply to an HTTP server.
var app = require('express')();
var http = require('http').Server(app);
var io = require('socket.io').listen(http);

//We define a route handler / that gets called when we hit our website home.
app.get('/', function(req, res){
	res.sendFile(__dirname + '/index.html');
});

io.of('/chat_room').on('connection', function(socket){
	console.log('a user connected');
	socket.on('disconnect', function(){
		console.log('user disconnected');
	});
	socket.on('chat_message', function(msg){
		socket.emit('chat message', msg);
		socket.broadcast.emit('chat message', msg);
		console.log('message: ' + msg);
	});
});

//We make the http server listen on port 3000.
http.listen(3000, function(){
	console.log('Listening on port 3000');
});