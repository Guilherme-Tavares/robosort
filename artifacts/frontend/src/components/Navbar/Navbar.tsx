import { Link, NavLink } from 'react-router-dom';
import './Navbar.css';

export function Navbar() {
  return (
    <header className="navbar">
      <div className="navbar-inner">
        {/* A marca repete o destino do botão Dashboard, de propósito:
            clicar no nome do sistema leva à tela inicial. */}
        <Link to="/dashboard" className="navbar-brand">
          RoboSort
        </Link>
        <nav className="navbar-links">
          <NavLink
            to="/dashboard"
            className={({ isActive }) => (isActive ? 'navbar-link navbar-link--active' : 'navbar-link')}
          >
            Dashboard
          </NavLink>
          <NavLink
            to="/compra"
            className={({ isActive }) => (isActive ? 'navbar-link navbar-link--active' : 'navbar-link')}
          >
            Compra
          </NavLink>
          <NavLink
            to="/fila"
            className={({ isActive }) => (isActive ? 'navbar-link navbar-link--active' : 'navbar-link')}
          >
            Separação
          </NavLink>
        </nav>
      </div>
    </header>
  );
}
