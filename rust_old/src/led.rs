use clap::{crate_version, App};
use rpi_led_matrix::{args, LedCanvas, LedMatrix};

pub fn initialize_led() -> (LedMatrix, LedCanvas) {
    println!("Initialize 2");
    let app = args::add_matrix_args(
        App::new("C++ Library Example")
        .about("shows basic usage of matrix arguments")
        .version(crate_version!())
    );
    println!("mat");
    let matches = app.get_matches();
    println!("get");
    let (options, rt_options) = args::matrix_options_from_args(&matches);
    
    println!("new");
    let matrix = LedMatrix::new(Some(options), Some(rt_options)).unwrap();
    println!("canv");
    let canvas = matrix.canvas();

    return (matrix, canvas);
}