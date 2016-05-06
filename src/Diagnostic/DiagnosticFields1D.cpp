
#include "DiagnosticFields1D.h"

#include "Params.h"
#include "Patch.h"
#include "Field1D.h"

using namespace std;

DiagnosticFields1D::DiagnosticFields1D( Params &params, SmileiMPI* smpi, Patch* patch, int ndiag )
    : DiagnosticFields( params, smpi, patch, ndiag )
{
    createPattern(params,patch);
}

DiagnosticFields1D::DiagnosticFields1D( DiagnosticFields* diag, Params &params, Patch* patch )
    : DiagnosticFields( diag, patch )
{
    createPattern(params,patch);
}

DiagnosticFields1D::~DiagnosticFields1D()
{
}

void DiagnosticFields1D::createPattern( Params& params, Patch* patch )
{
    std::vector<unsigned int> istart;
    istart = params.oversize;
    std::vector<unsigned int> bufsize;
    bufsize.resize(params.nDim_field, 0);

    for (unsigned int i=0 ; i<params.nDim_field ; i++) {
        if (patch->Pcoordinates[i]!=0) istart[i]+=1;
        bufsize[i] = params.n_space[i] + 1;
    }

    int nx0 = bufsize[0];

    // memspace [primDual]
    // fimespace[primDual]
    int nx;
    for (int ix_isPrim=0 ; ix_isPrim<2 ; ix_isPrim++) {
        nx = nx0 + ix_isPrim;

        istart = params.oversize;
        bufsize.resize(params.nDim_field, 0);

        for (unsigned int i=0 ; i<params.nDim_field ; i++) {
            if (patch->Pcoordinates[i]!=0) istart[i]+=1;
            bufsize[i] = params.n_space[i] + 1;
        }
        bufsize[0] += ix_isPrim;

        /*
         * Create the dataspace for the dataset.
         */
        hsize_t     chunk_dims[1];
        chunk_dims[0] = nx + 2*params.oversize[0] ;
        hid_t memspace  = H5Screate_simple(params.nDim_field, chunk_dims, NULL);

        hsize_t     offset[1];
        hsize_t     stride[1];
        hsize_t     count[1];

        offset[0] = istart[0];
        stride[0] = 1;

        if (params.number_of_patches[0] != 1) {
            if ( ix_isPrim == 0 ) {
                if (patch->Pcoordinates[0]!=0)
                    bufsize[0]--;
            }
            else {
                if ( (patch->Pcoordinates[0]!=0) && (patch->Pcoordinates[0]!=params.number_of_patches[0]-1) )
                    bufsize[0] -= 2;
                else
                    bufsize[0] -= 1;
            }
        }
        count[0] = bufsize[0];
        H5Sselect_hyperslab(memspace, H5S_SELECT_SET, offset, stride, count, NULL);
        memspace_ [ ix_isPrim ] = memspace;

        // Each process defines dataset in memory and writes it to the hyperslab
        // in the file.
        //
        hsize_t     dimsf[1];
        if (!params.nspace_win_x)
            dimsf[0] = params.n_space_global[0]+1+ix_isPrim;
        else
            dimsf[0] = params.nspace_win_x+1+ix_isPrim;

        hid_t filespace = H5Screate_simple(params.nDim_field, dimsf, NULL);
        //
        // Select hyperslab in the file.
        //
        //offset[0] = patch->getCellStartingGlobalIndex(0)+istart[0];
        offset[0] = patch->Pcoordinates[0]*params.n_space[0] - params.oversize[0]+istart[0];
        stride[0] = 1;
        count[0] = 1;
        hsize_t     block[1];
        block[0] = bufsize[0];
        H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, stride, count, block);
        filespace_[ ix_isPrim ] = filespace;

    }

} // END createPattern



void DiagnosticFields1D::updatePattern( Params& params, Patch* patch )
{
    for (int ix_isPrim=0 ; ix_isPrim<2 ; ix_isPrim++) {
        H5Sclose( memspace_ [ ix_isPrim ] );
        H5Sclose( filespace_[ ix_isPrim ] );
    }
    createPattern( params, patch );
}


// Write field in the time step dataset of the global file
void DiagnosticFields1D::writeField( Field* field, hid_t group_id )
{
    std::vector<unsigned int> isDual = field->isDual_;
    Field1D* f1D =  static_cast<Field1D*>(field);
    
    hid_t memspace  = memspace_ [ isDual[0] ];
    hid_t filespace = filespace_[ isDual[0] ];
    
    hid_t plist_id = H5Pcreate(H5P_DATASET_CREATE);
    //chunk_dims[0] = ???;
    //H5Pset_chunk(plist_id, 2, chunk_dims); // Problem different dims for each process
    //hid_t dset_id = H5Dcreate(file_id, (field->name).c_str(), H5T_NATIVE_DOUBLE, filespace, H5P_DEFAULT, plist_id, H5P_DEFAULT);
    //hid_t dset_id = H5Dcreate(group_id, (field->name).c_str(), H5T_NATIVE_DOUBLE, filespace, H5P_DEFAULT, plist_id, H5P_DEFAULT);
    hid_t dset_id;
    htri_t status = H5Lexists( group_id, (field->name).c_str(), H5P_DEFAULT ); 
    if (!status)
        dset_id  = H5Dcreate(group_id, (field->name).c_str(), H5T_NATIVE_DOUBLE, filespace, H5P_DEFAULT, plist_id, H5P_DEFAULT);
    else
        dset_id = H5Dopen(group_id, (field->name).c_str(), H5P_DEFAULT);                
    
    
    H5Pclose(plist_id);
    
    H5Dwrite( dset_id, H5T_NATIVE_DOUBLE, memspace, filespace, write_plist, &(f1D->data_[0]) );
    H5Dclose(dset_id);
    
}

//! this method writes a field on an hdf5 file should be used just for debug (doesn't use params.output_dir)
void DiagnosticFields1D::write( Field* field )
{
    std::vector<unsigned int> isDual = field->isDual_;
    Field1D* f1D =  static_cast<Field1D*>(field);
    string name = f1D->name+".h5";

    hid_t memspace  = memspace_ [ isDual[0] ];
    hid_t filespace = filespace_[ isDual[0] ];

    MPI_Info info  = MPI_INFO_NULL;
    hid_t plist_id = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_fapl_mpio(plist_id, MPI_COMM_WORLD, info);
    hid_t file_id = H5Fcreate( name.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, plist_id);
    H5Pclose(plist_id);

    plist_id = H5Pcreate(H5P_DATASET_CREATE);
    //        chunk_dims[0] = bufsize[0];
    //        H5Pset_chunk(plist_id, 2, chunk_dims); // Problem different dims for each process, may be the same for all ...
    hid_t dset_id = H5Dcreate(file_id, "Field", H5T_NATIVE_DOUBLE, filespace, H5P_DEFAULT, plist_id, H5P_DEFAULT);
    H5Pclose(plist_id);

    H5Dwrite( dset_id, H5T_NATIVE_DOUBLE, memspace, filespace, write_plist, &(f1D->data_[0]) );
    H5Dclose(dset_id);

    H5Fclose( file_id );

} // END write

