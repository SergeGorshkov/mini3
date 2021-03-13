<?php
ini_set("display_errors", "1");

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
echo("PUT prefixes\n");
$req = [ 'RequestId' => '1',
	 'Prefix' => [ 
		    [ '' => 'http://localhost/' ],
		    [ 'rdf' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#' ],
		    [ 'rdfs' => 'http://www.w3.org/2000/01/rdf-schema#' ],
		    [ 'owl' => 'http://www.w3.org/2002/07/owl#' ],
		    [ 'xsd' => 'http://www.w3.org/2001/XMLSchema#' ],
		    [ 'foaf' => 'http://xmlns.com/foaf/0.1/' ] ] ];
$st = microtime(true);
request( 'PUT', 'prefix', $req );
$et = microtime(true);
echo(($et-$st)."\n");

$fill = "";
/*for($a=0;$a<1000;$a++)
	$fill.="-"; */

// Send triples
$range = 100; $portion = 1000;
echo("PUT " . ($range * $portion) . " triples\n");
$st = microtime(true);
for( $i=0; $i<$range; $i++ ) {
	if($i % 100 == 0) echo("PUT ".$i."\n");
	$req = [    'RequestId' => "req".$i,
	            'Pattern' => []];
	for( $j=0; $j<$portion; $j++) {
		$p = [ $fill.'Subject'.($i*$portion+$j), $fill.'Predicate'.rand(), $fill.'Object'.rand() ];
		$req[ 'Pattern' ][] = $p;
		$patterns[] = $p;
	}
	$sti = microtime(true);
	$res = request( 'PUT', 'triple', $req );
if(!isset($res->Status) || $res->Status != "Ok")
	exit;

	$eti = microtime(true);
}
echo("\nLast " . $portion . " items insert time: ".($eti-$sti)."\n");
$pattern1 = [ '*', $req[ 'Pattern' ][ sizeof($req['Pattern']) - 1 ][1], $req[ 'Pattern' ][ sizeof($req['Pattern']) - 1 ][2] ];
$pattern2 = [ $req[ 'Pattern' ][ sizeof($req['Pattern']) - 2 ][0], '*', $req[ 'Pattern' ][ sizeof($req['Pattern']) - 2 ][2] ];

// Check that every added triple exists - this ensures that the index search works correctly
foreach($patterns as $i => $pattern) {
	$req = [ 'Pattern' => [ $pattern ] ];
	$res = request( 'GET', 'triple', $req );
	if(!isset($res->Triples) || sizeof($res->Triples) != 1) {
		echo("ERROR: triple " . $i . " not found " . print_r( $pattern, true ) . "\n");
		exit;
	}
}
echo("Database is consistent\n");

// Delete one triple
echo("DELETE triples\n");
$req = [    'RequestId' => '2',
	    'Pattern' => [
                $pattern1
            ]
            ];
$st = microtime(true);
request( 'DELETE', 'triple', $req );
$et = microtime(true);
echo("Delete time: ".($et-$st)."\n");

// Get amount of triples by pattern
$req = [    'RequestId' => '3',
	    'Pattern' => [
                $pattern2
            ]
            ];
echo("GET triples count\n");
$st = microtime(true);
$res = request( 'GET', 'triple/count', $req );
$et = microtime(true);
echo("Got " . $res->Count . " triples by pattern\n");
echo("Get count time: ".($et-$st)."\n");

// Get triples by pattern
$req = [    'RequestId' => '3',
	    'Pattern' => [
                $pattern2
            ]
            ];
echo("GET triples\n");
$st = microtime(true);
$res = request( 'GET', 'triple', $req );
$et = microtime(true);
if(!isset($res->Triples)) {
	echo("ERROR: cannot get triples by pattern\n");
	exit;
}
echo("Got " . sizeof($res->Triples) . " by pattern\n");
echo("Get time: ".($et-$st)."\n");

?>