// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Ym2203_Emu.h"
#include "fm.h"
#include <string.h>

static void psg_set_clock(void *param, int clock)
{
	Ym2203_Emu *info = (Ym2203_Emu *)param;
	info->psg_set_clock( clock );
}

static void psg_write(void *param, int address, int data)
{
	Ym2203_Emu *info = (Ym2203_Emu *)param;
	info->psg_write( address, data );
}

static int psg_read(void *param)
{
	Ym2203_Emu *info = (Ym2203_Emu *)param;
	return info->psg_read();
}

static void psg_reset(void *param)
{
	Ym2203_Emu *info = (Ym2203_Emu *)param;
	info->psg_reset();
}

static const ssg_callbacks psgintf =
{
	psg_set_clock,
	psg_write,
	psg_read,
	psg_reset
};

Ym2203_Emu::Ym2203_Emu() { opn = 0; psg.set_type( Ay_Apu::Ym2203 ); }

Ym2203_Emu::~Ym2203_Emu()
{
	if ( opn ) ym2203_shutdown( opn );
}

int Ym2203_Emu::set_rate( int sample_rate, int clock_rate )
{
	if ( opn )
	{
		ym2203_shutdown( opn );
		opn = 0;
	}
	
	opn = ym2203_init( this, clock_rate, sample_rate, &psgintf );
	if ( !opn )
		return 1;

	this->sample_rate = sample_rate;
	psg_clock = clock_rate * 2;

	buffer.set_sample_rate( sample_rate );
	buffer.clock_rate( psg_clock );

	psg.volume( 1.0 );
	
	reset();
	return 0;
}

void Ym2203_Emu::reset()
{
	psg.reset();
	ym2203_reset_chip( opn );
	mute_voices( 0 );
}

static stream_sample_t* DUMMYBUF[0x02] = {(stream_sample_t*)NULL, (stream_sample_t*)NULL};

void Ym2203_Emu::write( int addr, int data )
{
	ym2203_update_one( opn, DUMMYBUF, 0 );
	ym2203_write( opn, 0, addr );
	ym2203_write( opn, 1, data );
}

void Ym2203_Emu::mute_voices( int mask )
{
	ym2203_set_mutemask( opn, mask );
	for ( unsigned i = 0, j = 1 << 3; i < 3; i++, j <<= 1)
	{
		Blip_Buffer * buf = ( mask & j ) ? NULL : &buffer;
		psg.set_output( i, buf );
	}
}

void Ym2203_Emu::run( int pair_count, sample_t* out )
{
	blip_sample_t buf[ 1024 ];
	FMSAMPLE bufL[ 1024 ];
	FMSAMPLE bufR[ 1024 ];
	FMSAMPLE * buffers[2] = { bufL, bufR };

	blip_time_t psg_end_time = pair_count * psg_clock / sample_rate;
	psg.end_frame( psg_end_time );
	buffer.end_frame( psg_end_time );

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		ym2203_update_one( opn, buffers, todo );

		int sample_count = buffer.read_samples( buf, todo );
		memset( buf + sample_count, 0, ( todo - sample_count ) * sizeof( blip_sample_t ) );

		for (int i = 0; i < todo; i++)
		{
			int output_l, output_r;
			int output = buf [i];
			output_l = output + bufL [i];
			output_r = output + bufR [i];
			output_l += out [0];
			output_r += out [1];
			if ( (short)output_l != output_l ) output_l = 0x7FFF ^ ( output_l >> 31 );
			if ( (short)output_r != output_r ) output_r = 0x7FFF ^ ( output_r >> 31 );
			out [0] = output_l;
			out [1] = output_r;
			out += 2;
		}

		pair_count -= todo;
	}
}

void Ym2203_Emu::psg_set_clock( int clock )
{
	psg_clock = clock * 2;
	buffer.clock_rate( clock );
}

void Ym2203_Emu::psg_write( int addr, int data )
{
    if ( !(addr & 1) ) psg.write_addr( data );
    else psg.write_data( 0, data );
}

int Ym2203_Emu::psg_read()
{
	return psg.read();
}

void Ym2203_Emu::psg_reset()
{
	psg.reset();
}
