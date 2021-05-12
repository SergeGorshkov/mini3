<?php

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
    return $response;
}

$req = [ 'RequestId' => '0',
	 'Prefix' => [ 
		    [ 'foaf' => 'http://xmlns.com/foaf/0.1/' ],
		    [ 'rdf' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#' ],
		    [ 'rdfs' => 'http://www.w3.org/2000/01/rdf-schema#' ],
		    [ 'owl' => 'http://www.w3.org/2002/07/owl#' ],
		    [ 'xsd' => 'http://www.w3.org/2001/XMLSchema#' ],
		    [ 'swrc' => 'http://swrc.ontoware.org/ontology#' ],
		    [ 'bench' => 'http://localhost/vocabulary/bench/' ],
		    [ 'dc' => 'http://purl.org/dc/elements/1.1/' ],
		    [ 'dcterms' => 'http://purl.org/dc/terms/' ],
		    [ 'person' => 'http://localhost/persons/' ] ] ];

$st = microtime(true);
request( 'PUT', 'prefix', $req );

$req = [    'RequestId' => '7',
	    'Chain' => [
                [ '?article1', 'rdf:type', 'bench:Article' ],
                [ '?article2', 'rdf:type', 'bench:Article' ],
                [ '?article1', 'dc:creator', '?author1' ],
                [ '?author1', 'foaf:name', '?name1' ],
                [ '?article2', 'dc:creator', '?author2' ],
                [ '?author2', 'foaf:name', '?name2' ],
                [ '?article1', 'swrc:journal', '?journal' ],
                [ '?article2', 'swrc:journal', '?journal' ]
            ],
            'Filter' => [
        	'and',
        	[ 'and',
        	    [ '?name1', 'less', '?name2' ]
        	]
            ]
            ];

$st = microtime(true);
$res_raw = request('GET', 'chain', $req );
$et = microtime(true);
$res = json_decode($res_raw);
if($res->Status == 'Ok') echo("OK ".sizeof($res->Result)."\n");
else echo("ERROR!\n".$res_raw."\n");
echo(" ".round($et-$st,3)."\n");

?>