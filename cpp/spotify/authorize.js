const express = require("express")
const querystring = require("querystring")

const client_id = process.env.SPOTIFY_CLIENT_ID;
const client_secret = process.env.SPOTIFY_CLIENT_SECRET;
const redirect_uri = 'http://10.6.0.23:8888/callback';



function makeid(length) {
    let result = '';
    const characters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    const charactersLength = characters.length;
    let counter = 0;
    while (counter < length) {
        result += characters.charAt(Math.floor(Math.random() * charactersLength));
        counter += 1;
    }
    return result;
}

const app = express();

app.get('/login', function(req, res) {

    var state = makeid(16);
    var scope = 'user-read-playback-state user-read-currently-playing';

    res.redirect('https://accounts.spotify.com/authorize?' +
        querystring.stringify({
            response_type: 'code',
            client_id: client_id,
            scope: scope,
            redirect_uri: redirect_uri,
            state: state
        }));
});

app.get('/callback', async function(req, res) {
    const fetch = (await import("node-fetch")).default

    const code = req.query.code || null;
    const state = req.query.state || null;

    if (state === null) {
        res.redirect('/#' +
            querystring.stringify({
                error: 'state_mismatch'
            }));
    } else {
        const params = new URLSearchParams()
        params.append("code", code)
        params.append("redirect_uri", redirect_uri)
        params.append("grant_type", "authorization_code")

        const response = await fetch("https://accounts.spotify.com/api/token", {
            method: "POST",
            body: params,
            headers: {
                'Authorization': 'Basic ' + (new Buffer.from(client_id + ':' + client_secret).toString('base64'))
            }
        }).then(e => e.json())

        console.log(JSON.stringify(response))

        res.send({success: true})
        process.exit(0)
    }
});

const port = parseInt(process.argv[2]);
app.listen(port, () => {})