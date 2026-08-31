use crossterm::{
    event::{self, Event, KeyCode},
    execute,
    terminal::{disable_raw_mode, enable_raw_mode,
               EnterAlternateScreen, LeaveAlternateScreen},
};
use ratatui::{Terminal, backend::CrosstermBackend};
use std::io::{self, stdout};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // setu// in main(), before enable_raw_mode
    let default_panic = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |info| {
        let _ = disable_raw_mode();
        let _ = execute!(stdout(), LeaveAlternateScreen);
        default_panic(info);
    }));
    enable_raw_mode()?;
    execute!(stdout(), EnterAlternateScreen)?;
    let backend = CrosstermBackend::new(stdout());
    let mut terminal = Terminal::new(backend)?;

    // run — catch any error so we can still clean up
    let result = run(&mut terminal);

    // teardown — always runs, even if run() errored
    disable_raw_mode()?;
    execute!(terminal.backend_mut(), LeaveAlternateScreen)?;

    result
}

fn run(terminal: &mut Terminal<CrosstermBackend<io::Stdout>>)
    -> Result<(), Box<dyn std::error::Error>>
{
    loop {
        terminal.draw(|_frame| {
            // placeholder — blank screen for now
        })?;

        if let Event::Key(key) = event::read()? {
            if key.code == KeyCode::Char('q') {
                break;
            }
        }
    }
    Ok(())
}
