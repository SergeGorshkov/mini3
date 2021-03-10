<?php

function request( $method, $endpoint, $query ) {
    $context = stream_context_create( array(
        'http' => array(
            'method' => $method,
            'header' => 'Content-Type: application/x-www-form-urlencoded' . PHP_EOL,
            'content' => 'request=' . json_encode( $query ),
        ),
    ) );
    $response = file_get_contents( "http://localhost:8081/" . $endpoint, false, $context );
//echo("Return: ".$response."\n");
    return json_decode( $response );
}

// Send prefixes
echo("1. PUT prefixes: ");
$req = [ 'RequestId' => '1',
	 'Prefix' => [
		    [ '' => 'http://localhost/' ],
		    [ 'rdf' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#' ],
		    [ 'rdfs' => 'http://www.w3.org/2000/01/rdf-schema#' ],
		    [ 'owl' => 'http://www.w3.org/2002/07/owl#' ],
		    [ 'xsd' => 'http://www.w3.org/2001/XMLSchema#' ],
		    [ 'foaf' => 'http://xmlns.com/foaf/0.1/' ],
		    [ 'custom' => 'http://some.prefix' ] ] ];
$st = microtime(true);
$res = request( 'PUT', 'prefix', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

/*
// Delete one prefix
echo("2. DELETE prefixes: ");
$req = [ 'RequestId' => '2',
	 'Prefix' => [
		    [ 'custom' => 'http://some.prefix' ] ] ];
$st = microtime(true);
$res = request( 'DELETE', 'prefix', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

// Get remaining prefixes
echo("3. GET prefixes: ");
$st = microtime(true);
$res = request( 'GET', 'prefix', [ 'RequestId' => '3' ] );
$et = microtime(true);
if($res->Status == 'Ok' && isset($res->Prefixes) && sizeof($res->Prefixes) == 6) echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");
*/

// Send some triples
$req = [    'RequestId' => '4',
	    'Pattern' => [
                [ 'Jack', 'knows', 'PHP' ],
                [ 'Jack', 'knows', 'JavaScript' ],
                [ 'Jack', 'knows', 'C++' ],
                [ 'Jane', 'knows', 'C++' ],
            ]
            ];
echo("4. PUT triples: ");
$st = microtime(true);
$res = request( 'PUT', 'triple', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

// Delete some triples
echo("5. DELETE triples: ");
$req = [    'RequestId' => '5',
	    'Pattern' => [
                [ '*', 'knows', 'JavaScript' ]
            ]
            ];

$st = microtime(true);
$res = request( 'DELETE', 'triple', $req );
$et = microtime(true);
if($res->Status == 'Ok') echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

// Get deleted triples (shall return nothing)
$req = [    'RequestId' => '6',
	    'Pattern' => [
                [ 'Serge', 'knows', 'JavaScript' ]
            ]
            ];
echo("6. GET deleted triples: ");
$st = microtime(true);
$res = request( 'GET', 'triple', $req );
$et = microtime(true);
if($res->Status == 'Ok' && !isset($res->Triples)) echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

// Get triples count
$req = [    'RequestId' => '7',
	    'Pattern' => [
                [ '*', '*', '*' ]
            ]
            ];
echo("7. GET triples count: ");
$st = microtime(true);
$res = request( 'GET', 'triple/count', $req );
$et = microtime(true);
if($res->Status == 'Ok' && $res->Count == 3) echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

// Get the remaining triples
$req = [    'RequestId' => '8',
	    'Pattern' => [
                [ '*', '*', '*' ]
            ]
            ];
echo("8. GET remaining triples: ");
$st = microtime(true);
$res = request( 'GET', 'triple', $req );
$et = microtime(true);
if($res->Status == 'Ok' && sizeof($res->Triples) == 3) echo("OK");
else echo("ERROR!");
echo(" ".round($et-$st,3)."\n");

?>