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
print_r(request( 'PUT', 'prefix', $req ));
$et = microtime(true);
echo(($et-$st)."\n");

// Populate database with triples
$req = [    'RequestId' => '1',
	    'Pattern' => [
		[ 'Person', 'rdf:type', 'owl:Class' ],
		[ 'Language', 'rdf:type', 'owl:Class' ],
		[ 'Programmer', 'rdfs:subClassOf', 'Person' ],
		[ 'LanguageType', 'rdf:type', 'owl:Class' ],
		[ 'hasType', 'rdf:type', 'owl:ObjectProperty' ],
		[ 'hasType', 'rdfs:domain', 'Language' ],
		[ 'hasType', 'rdfs:range', 'LanguageType' ],
		[ 'knows', 'rdf:type', 'owl:ObjectProperty' ],
		[ 'knows', 'rdfs:domain', 'Person' ],
		[ 'knows', 'rdfs:range', 'Language' ],
		[ 'isKnownBy', 'rdf:type', 'owl:ObjectProperty' ],
		[ 'isKnownBy', 'rdfs:domain', 'Language' ],
		[ 'isKnownBy', 'rdfs:range', 'Person' ],

		[ 'Interpretable', 'rdf:type', 'LanguageType' ],
		[ 'Compilable', 'rdf:type', 'LanguageType' ],
		[ 'Interpretable', 'rdfs:label', 'Interpretable', 'xsd:string' ],
		[ 'Compilable', 'rdfs:label', 'Compilable', 'xsd:string' ],
		[ 'Compilable', 'rdfs:label', 'Compilable to binary code', 'xsd:string' ],
		[ 'Jack', 'rdf:type', 'Person' ],
		[ 'Jack', 'rdfs:label', 'Jack Doe', 'xsd:string', 'EN' ],
		[ 'Jack', 'rdfs:label', 'Джек Доу', 'xsd:string', 'RU' ],
		[ 'Jane', 'rdf:type', 'Person' ],
		[ 'Jane', 'rdfs:label', 'Jane Doe', 'xsd:string', 'EN' ],
		[ 'PHP', 'rdf:type', 'Language' ],
		[ 'PHP', 'rdfs:label', 'PHP', 'xsd:string' ],
		[ 'PHP', 'hasType', 'Interpretable' ],
		[ 'JavaScript', 'rdf:type', 'Language' ],
		[ 'JavaScript', 'rdfs:label', 'JavaScript', 'xsd:string' ],
		[ 'JavaScript', 'hasType', 'Interpretable' ],
		[ 'C++', 'rdf:type', 'Language' ],
		[ 'C++', 'rdfs:label', 'C++', 'xsd:string' ],
		[ 'C++', 'hasType', 'Compilable' ],
                [ 'Jack', 'knows', 'PHP' ],
                [ 'Jack', 'knows', 'JavaScript' ],
                [ 'Jack', 'knows', 'C++' ],
                [ 'C++', 'isKnownBy', 'Jack' ],
                [ 'Jane', 'knows', 'C++' ],
                [ 'C++', 'isKnownBy', 'Jane' ]
            ]
            ];
echo("PUT triples\n");
print_r(request( 'PUT', 'triple', $req ));

$req = [    'RequestId' => '3',
	    'Chain' => [
                [ 'Jack', 'knows', '?lang' ],
                [ '?lang', 'hasType', '?type' ],
                [ '?type', 'rdfs:label', '?typename' ],
                [ '?person', 'rdfs:label', '?personname' ],
                [ '?lang', 'isKnownBy', '?person' ],
            ],
            'Order' => [ [ '?person', 'desc' ],
        		 [ '?personname', 'asc' ]
            ],
            'Filter' => [
        	'and',
        	[ 'or',
        	    [ '?personname', 'contains', 'Jack' ],
        	    [ '?personname', 'contains', 'Сергей' ],
        	],
        	[ 'and',
        	    [ '?lang', 'equal', 'C++' ]
        	]
            ]
            ];

echo("GET chain\n");
$st = microtime(true);
$res = request('GET', 'chain', $req );
$et = microtime(true);
echo("GET chain: ".($et-$st)."\n");

// Print table
foreach($res->Vars as $rr) {
    echo($rr."\t");
}
echo("\n");
foreach($res->Result as $rr) {
    foreach($rr as $r) {
	echo($r[0]."\t");
    }
    echo("\n");
}
exit;

// Get triples by pattern
$req = [	'RequestId' => '5',
		'Pattern' => [
			[ 'Jack', "*", "*" ]
		],
		'Order' => [
			[ 'predicate', 'desc' ],
			[ 'object', 'asc' ]
		]
	];
echo("GET triple\n");
$res = request('GET', 'triple', $req );
print_r($res);

?>
