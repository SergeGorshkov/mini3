<?php
ini_set("error_reporting", E_ERROR);

function request( $method, $endpoint, $query ) {
    $context = stream_context_create( array(
        'http' => array(
            'method' => $method,
            'header' => 'Content-Type: application/x-www-form-urlencoded' . PHP_EOL,
            'content' => 'request=' . json_encode( $query ),
        ),
    ) );
    $response = file_get_contents( "http://localhost:8082/" . $endpoint, false, $context );
//echo("Return: ".$response."\n");
    return json_decode( $response );
}

$n = 0;
$fp = fopen("berlin.nq", "r");
while( $s = fgets($fp) ) {
	if( $n % 10 == 0 ) {
		if( $n != 0 ) {
echo($n."\n");
			$res = request( 'PUT', 'triple', $req );
			if($res->Status == 'Ok') echo("OK\n");
			else echo("Error\n");
		}
		$req = [ 'RequestId' => $n, 'Pattern' => [] ];
	}
	preg_match('/<([^>]+)> <([^>]+)> [<\"]([^>\"]*)([>\"])(?:@(..))*(?:\^\^<(.*)>)*/', $s, $matches);
	$p = [ $matches[1], $matches[2], $matches[3] ];
	if( isset($matches[6]) ) $p[] = $matches[6];
	else {
	    if( isset($matches[5]) || $matches[4] == '"' ) $p[] = "http://www.w3.org/2001/XMLSchema#string";
	}
	if( isset($matches[5]) ) $p[] = $matches[5];
	$req[ 'Pattern' ][] = $p;
	$n++;
}
$res = request( 'PUT', 'triple', $req );
if($res->Status == 'Ok') echo("OK\n");
else echo("Error\n");

?>