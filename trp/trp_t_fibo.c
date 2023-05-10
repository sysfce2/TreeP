/*
    TreeP Run Time Support
    Copyright (C) 2008-2023 Frank Sinapsi

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "trp.h"

static uns8b trp_fibo_check( trp_obj_t *h );
static uns8b trp_fibo_node_check( trp_obj_t *h );
static uns8b trp_fibo_node_connected_check( trp_obj_t *h );
static uns8b trp_fibo_cmp( trp_fibo_t *h, trp_obj_t *x, trp_obj_t *y );
static void trp_fibo_insert_low( trp_obj_t *h, trp_fibo_node_t *x );
static void trp_fibo_link( trp_fibo_node_t *y, trp_fibo_node_t *x );
static void trp_fibo_consolidate( trp_fibo_t *h );
static void trp_fibo_cut( trp_fibo_t *h, trp_fibo_node_t *x, trp_fibo_node_t *y );
static void trp_fibo_cascading_cut( trp_fibo_t *h, trp_fibo_node_t *y );

uns8b trp_fibo_print( trp_print_t *p, trp_fibo_t *obj )
{
    uns8b buf[ 11 ];

    if ( trp_print_char_star( p, "#fibo " ) )
        return 1;
    if ( obj->sottotipo ) {
        if ( trp_print_char_star( p, "node (children=" ) )
            return 1;
        sprintf( buf, "%u", ((trp_fibo_node_t *)obj)->degree );
        if ( trp_print_char_star( p, buf ) )
            return 1;
    } else {
        if ( trp_print_char_star( p, "heap (length=" ) )
            return 1;
        sprintf( buf, "%u", obj->n );
        if ( trp_print_char_star( p, buf ) )
            return 1;
    }
    return trp_print_char_star( p, ")#" );
}

trp_obj_t *trp_fibo_length( trp_fibo_t *obj )
{
    if ( obj->sottotipo )
        return ((trp_fibo_node_t *)obj)->key;
    return trp_sig64( obj->n );
}

trp_obj_t *trp_fibo_width( trp_fibo_node_t *obj )
{
    if ( obj->sottotipo )
        return obj->obj;
    return UNDEF;
}

trp_obj_t *trp_fibo( trp_obj_t *cmp )
{
    trp_fibo_t *h;

    if ( cmp ) {
        if ( cmp->tipo != TRP_FUNPTR )
            return UNDEF;
        if ( ((trp_funptr_t *)cmp)->nargs != 2 )
            return UNDEF;
    } else
        cmp = trp_funptr_less_obj();

    h = trp_gc_malloc( sizeof( trp_fibo_t ) );
    h->tipo = TRP_FIBO;
    h->sottotipo = 0;
    h->n = 0;
    h->min = NULL;
    h->cmp = ( (trp_funptr_t *)cmp )->f;
    return (trp_obj_t *)h;
}

static uns8b trp_fibo_check( trp_obj_t *h )
{
    if ( h->tipo != TRP_FIBO )
        return 1;
    return ( ((trp_fibo_t *)h)->sottotipo ) ? 1 : 0;
}
static uns8b trp_fibo_node_check( trp_obj_t *h )
{
    if ( h->tipo != TRP_FIBO )
        return 1;
    return ( ((trp_fibo_t *)h)->sottotipo ) ? 0 : 1;
}

static uns8b trp_fibo_node_connected_check( trp_obj_t *h )
{
    if ( h->tipo != TRP_FIBO )
        return 1;
    if ( ((trp_fibo_t *)h)->sottotipo )
        return ( ((trp_fibo_node_t *)h)->marked < 2 ) ? 0 : 1;
    return 1;
}

static uns8b trp_fibo_cmp( trp_fibo_t *h, trp_obj_t *x, trp_obj_t *y )
{
    if ( x == NULL )
        return 1;
    if ( y == NULL )
        return 0;
    return ( ( h->cmp )( x, y ) == TRP_TRUE ) ? 1 : 0;
}

trp_obj_t *trp_fibo_first( trp_obj_t *h )
{
    if ( trp_fibo_check( h ) )
        return UNDEF;
    h = (trp_obj_t *)( ((trp_fibo_t *)h)->min );
    return h ? h : UNDEF;
}

trp_obj_t *trp_fibo_key( trp_obj_t *x )
{
    if ( trp_fibo_node_check( x ) )
        return UNDEF;
    return ((trp_fibo_node_t *)x)->key;
}

trp_obj_t *trp_fibo_obj( trp_obj_t *x )
{
    if ( trp_fibo_node_check( x ) )
        return UNDEF;
    return ((trp_fibo_node_t *)x)->obj;
}

trp_obj_t *trp_fibo_insert( trp_obj_t *h, trp_obj_t *key, trp_obj_t *obj )
{
    trp_fibo_node_t *x;

    if ( trp_fibo_check( h ) )
        return UNDEF;
    x = trp_gc_malloc( sizeof( trp_fibo_node_t ) );
    x->tipo = TRP_FIBO;
    x->sottotipo = 1;
    x->key = key;
    x->obj = obj ? obj : UNDEF;
    trp_fibo_insert_low( h, x );
    return (trp_obj_t *)x;
}

static void trp_fibo_insert_low( trp_obj_t *h, trp_fibo_node_t *x )
{
    trp_fibo_node_t *min;

    x->p = NULL;
    x->child = NULL;
    x->left = NULL;
    x->right = NULL;
    x->degree = 0;
    x->marked = 0;
    // concatenate the root list containing x with root list h
    if ( min = ((trp_fibo_t *)h)->min ) {
        trp_fibo_node_t *min_right = min->right;
        x->left = min;
        x->right = min_right;
        min->right = x;
        min_right->left = x;
        if ( trp_fibo_cmp( (trp_fibo_t *)h, x->key, min->key )  )
            ((trp_fibo_t *)h)->min = x;
    } else {
        x->left = x;
        x->right = x;
        ((trp_fibo_t *)h)->min = x;
    }
    ((trp_fibo_t *)h)->n++;
}

// make y a child of x
static void trp_fibo_link( trp_fibo_node_t *y, trp_fibo_node_t *x )
{
    trp_fibo_node_t *child;

    // remove y from the root list of h
    y->left->right = y->right;
    y->right->left = y->left;

    // make y a child of x, incrementing x.degree
    if ( ( child = x->child ) == NULL ) {
        x->child = y;
        y->left = y;
        y->right = y;
    } else {
        y->right = child->right;
        child->right->left = y;
        y->left = child;
        child->right = y;
    }
    y->p = x;
    x->degree++;
    y->marked = 0;
}

static void trp_fibo_consolidate( trp_fibo_t *h )
{
    int dn = (int)( log( h->n ) / log( 2 ) ) + 1, i, d;
    trp_obj_t *min_key;
    trp_fibo_node_t *w  = h->min; /* the first node we will consolidate */
    trp_fibo_node_t *f = w->left; /* the final node in this heap we will consolidate */
    trp_fibo_node_t *x = NULL;
    trp_fibo_node_t *y = NULL;
    trp_fibo_node_t *t = NULL; /* temp */
    trp_fibo_node_t *A[ dn ];

    for ( i = 0 ; i < dn ; ++i )
        A[ i ] = NULL;

    while ( w != f ) {
        d = w->degree;
        x = w;
        w = w->right;
        while ( A[ d ] ) {
            // another node with the same degree as x
            y = A[ d ];
            if( trp_fibo_cmp( h, y->key, x->key ) ) {
                t = x;
                x = y;
                y = t;
            }
            trp_fibo_link( y, x );
            A[ d++ ] = NULL;
        }
        A[ d ] = x;
    }

    // the last node to consolidate (f == w)
    d = w->degree;
    x = w;
    while ( A[ d ] ) {
        // another node with the same degree as x
        y = A[ d ];
        if( trp_fibo_cmp( h, y->key, x->key ) ) {
            t = x;
            x = y;
            y = t;
        }
        trp_fibo_link( y, x );
        A[ d++ ] = NULL;
    }
    A[ d ] = x;

    h->min = NULL;
    // to get min in this heap
    for ( i = 0 ; i < dn ; ++i )
        if ( A[ i ] )
            if ( ( h->min == NULL ) || trp_fibo_cmp( h, A[ i ]->key, min_key ) ) {
                h->min = A[ i ];
                min_key = A[ i ]->key;
            }
}

trp_obj_t *trp_fibo_extract( trp_obj_t *h )
{
    trp_fibo_node_t *z, *firstChid;

    if ( trp_fibo_check( h ) )
        return UNDEF;
    if ( ( z = ((trp_fibo_t *)h)->min ) == NULL )
        return UNDEF;
    firstChid = z->child;
    // add the children of minimum node to the root list.
    if ( firstChid ) {
        trp_fibo_node_t * sibling = firstChid->right;
        // min_right point the right node of minimum node
        trp_fibo_node_t * min_right = z->right;

        // add the first child to the root list
        z->right = firstChid;
        firstChid->left = z;
        min_right->left = firstChid;
        firstChid->right = min_right;

        firstChid->p = NULL;
        min_right = firstChid;
        while ( firstChid != sibling ) {
            // record the right sibling of sibling
            trp_fibo_node_t *sibling_right = sibling->right;

            z->right = sibling;
            sibling->left = z;
            sibling->right = min_right;
            min_right->left = sibling;

            min_right = sibling;
            sibling = sibling_right;

            // update the p
            sibling->p = NULL;
        }
    }
    // remove z from the root list
    z->left->right = z->right;
    z->right->left = z->left;

    // the root list has only one node
    if ( z == z->right ) {
        ((trp_fibo_t *)h)->min = NULL;

        // the children of z shoud be the root list of the heap
        // and find the minimum in this heap
        if ( z->child ) {
            trp_fibo_node_t *child = z->child;
            ((trp_fibo_t *)h)->min = child;
            trp_fibo_node_t *sibling = child->right;
            while ( child != sibling ) {
                if ( trp_fibo_cmp( (trp_fibo_t *)h, sibling->key, ((trp_fibo_t *)h)->min->key ) )
                    ((trp_fibo_t *)h)->min = sibling;
                sibling = sibling->right;
            }
        }
    } else {
        ((trp_fibo_t *)h)->min = z->right;
        trp_fibo_consolidate( ((trp_fibo_t *)h) );
    }
    ((trp_fibo_t *)h)->n--;
    z->p = NULL;
    z->child = NULL;
    z->left = NULL;
    z->right = NULL;
    z->degree = 0;
    z->marked = 2;
    return (trp_obj_t *)z;
}

// if we cut node x from y, it indicates root list of
// h not null
static void trp_fibo_cut( trp_fibo_t *h, trp_fibo_node_t *x, trp_fibo_node_t *y )
{
    // remove x from child list of y, decrementing degree[y]
    if( y->degree == 1 ) {
        y->child = NULL;
    } else {
        x->left->right = x->right;
        x->right->left = x->left;

        // update child[y]
        y->child = x->right;
    }

    // add x to the root list of h
    x->left = h->min;
    x->right = h->min->right;
    h->min->right = x;
    x->right->left = x;

    // updating p[x], marked[x] and decrementing degree[y]
    x->p = NULL;
    x->marked = 0;
    y->degree--;
}

static void trp_fibo_cascading_cut( trp_fibo_t *h, trp_fibo_node_t *y )
{
    trp_fibo_node_t *z;

    if ( z = y->p )
        if ( y->marked ) {
            trp_fibo_cut( h, y, z );
            trp_fibo_cascading_cut( h, z );
        } else
            y->marked = 1;
}

uns8b trp_fibo_decrease_key( trp_obj_t *h, trp_obj_t *x, trp_obj_t *key )
{
    trp_fibo_node_t *y;

    if ( trp_fibo_check( h ) || trp_fibo_node_connected_check( x ) )
        return 1;
    if ( trp_fibo_cmp( (trp_fibo_t *)h, key, ((trp_fibo_node_t *)x)->key ) == 0 )
        return 1;
    ((trp_fibo_node_t *)x)->key = key;
    // if x->key >= y->key, do nothing
    // else we need cut
    if ( y = ((trp_fibo_node_t *)x)->p )
        if ( trp_fibo_cmp( (trp_fibo_t *)h, ((trp_fibo_node_t *)x)->key, y->key ) ) {
            trp_fibo_cut( (trp_fibo_t *)h, ((trp_fibo_node_t *)x), y );
            trp_fibo_cascading_cut( (trp_fibo_t *)h, y );
        }
    // get min node of this heap
    if ( trp_fibo_cmp( (trp_fibo_t *)h, ((trp_fibo_node_t *)x)->key, ((trp_fibo_t *)h)->min->key ) )
        ((trp_fibo_t *)h)->min = ((trp_fibo_node_t *)x);
    return 0;
}

uns8b trp_fibo_delete( trp_obj_t *h, trp_obj_t *x )
{
    trp_obj_t *key;

    if ( trp_fibo_node_connected_check( x ) )
        return 1;
    key = ((trp_fibo_node_t *)x)->key;
    if ( trp_fibo_decrease_key( h, x, NULL ) )
        return 1;
    (void)trp_fibo_extract( h );
    ((trp_fibo_node_t *)x)->key = key;
    return 0;
}

uns8b trp_fibo_set_key( trp_obj_t *h, trp_obj_t *x, trp_obj_t *key )
{
    if ( trp_fibo_decrease_key( h, x, key ) == 0 )
        return 0;
    if ( trp_fibo_decrease_key( h, x, NULL ) )
        return 1;
    (void)trp_fibo_extract( h );
    ((trp_fibo_node_t *)x)->key = key;
    trp_fibo_insert_low( h, (trp_fibo_node_t *)x );
    return 0;
}

/*

trp_fibo_t *heapUnion( trp_fibo_t *h1, trp_fibo_t *h2 )
{
    trp_fibo_t *h = makeFiboHeap();

    if ( h1->min == NULL ) {
        h->min = h2->min;
    } else if ( h2->min == NULL ) {
        h->min = h1->min;
    } else {
        trp_fibo_node_t *min_h1 = h1->min;
        trp_fibo_node_t *min_right_h1 = min_h1->right;
        trp_fibo_node_t *min_h2 = h2->min;
        trp_fibo_node_t *min_right_h2 = min_h2->right;

        min_h1->right = min_right_h2;
        min_right_h2->left = min_h1;

        min_h2->right = min_right_h1;
        min_right_h1->left = min_h2;

        h->min = ( h1->min->key > h2->min->key ) ? h2->min : h1->min;
    }
    trp_gc_free( h1 );
    trp_gc_free( h2 );
    h->n = h1->n + h2->n;
    return h;
}

*/

